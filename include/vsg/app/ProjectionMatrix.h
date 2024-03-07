#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/EllipsoidModel.h>
#include <vsg/app/ViewMatrix.h>
#include <vsg/io/Logger.h>

namespace vsg
{

    /// ProjectionMatrix is a base class for specifying the Camera projection matrix and its inverse.
    class VSG_DECLSPEC ProjectionMatrix : public Inherit<Object, ProjectionMatrix>
    {
    public:
        virtual dmat4 transform() const = 0;

        virtual dmat4 inverse() const
        {
            return vsg::inverse(transform());
        }

        virtual void changeExtent(const VkExtent2D& /*prevExtent*/, const VkExtent2D& /*newExtent*/)
        {
        }
    };
    VSG_type_name(vsg::ProjectionMatrix);

    /// Perspective is a ProjectionMatrix that implements the gluPerspective model for setting the projection matrix.
    class VSG_DECLSPEC Perspective : public Inherit<ProjectionMatrix, Perspective>
    {
    public:
        Perspective() :
            fieldOfViewY(60.0),
            aspectRatio(1.0),
            nearDistance(1.0),
            farDistance(10000.0)
        {
        }

        Perspective(double fov, double ar, double nd, double fd) :
            fieldOfViewY(fov),
            aspectRatio(ar),
            nearDistance(nd),
            farDistance(fd)
        {
        }

        dmat4 transform() const override { return perspective(radians(fieldOfViewY), aspectRatio, nearDistance, farDistance); }

        void changeExtent(const VkExtent2D& prevExtent, const VkExtent2D& newExtent) override
        {
            double oldRatio = static_cast<double>(prevExtent.width) / static_cast<double>(prevExtent.height);
            double newRatio = static_cast<double>(newExtent.width) / static_cast<double>(newExtent.height);

            aspectRatio *= (newRatio / oldRatio);
        }

        double fieldOfViewY;
        double aspectRatio;
        double nearDistance;
        double farDistance;

    public:
        void read(Input& input) override;
        void write(Output& output) const override;
    };
    VSG_type_name(vsg::Perspective);

    /// Orthographic is a ProjectionMatrix that implements the glOrtho model for setting the projection matrix.
    class Orthographic : public Inherit<ProjectionMatrix, Orthographic>
    {
    public:
        Orthographic() :
            left(-1.0),
            right(1.0),
            bottom(-1.0),
            top(1.0),
            nearDistance(1.0),
            farDistance(10000.0)
        {
        }

        Orthographic(double l, double r, double b, double t, double nd, double fd) :
            left(l),
            right(r),
            bottom(b),
            top(t),
            nearDistance(nd),
            farDistance(fd)
        {
        }

        dmat4 transform() const override { return orthographic(left, right, bottom, top, nearDistance, farDistance); }

        void changeExtent(const VkExtent2D& prevExtent, const VkExtent2D& newExtent) override
        {
            double oldRatio = static_cast<double>(prevExtent.width) / static_cast<double>(prevExtent.height);
            double newRatio = static_cast<double>(newExtent.width) / static_cast<double>(newExtent.height);
            left *= newRatio / oldRatio;
            right *= newRatio / oldRatio;
        }

        double left;
        double right;
        double bottom;
        double top;
        double nearDistance;
        double farDistance;

    public:
        void read(Input& input) override;
        void write(Output& output) const override;
    };
    VSG_type_name(vsg::Orthographic);

    /// RelativeProjection is a ProjectionMatrix that decorates another ProjectionMatrix and pre-multiplies its transform matrix to give a relative projection matrix.
    class RelativeProjection : public Inherit<ProjectionMatrix, RelativeProjection>
    {
    public:
        RelativeProjection(const dmat4& m, ref_ptr<ProjectionMatrix> pm) :
            projectionMatrix(pm),
            matrix(m)
        {
        }

        /// returns matrix * projectionMatrix->transform()
        dmat4 transform() const override
        {
            return matrix * projectionMatrix->transform();
        }

        void changeExtent(const VkExtent2D& prevExtent, const VkExtent2D& newExtent) override
        {
            double oldRatio = static_cast<double>(prevExtent.width) / static_cast<double>(prevExtent.height);
            double newRatio = static_cast<double>(newExtent.width) / static_cast<double>(newExtent.height);
            matrix = scale(oldRatio / newRatio, 1.0, 1.0) * matrix;
        }
        ref_ptr<ProjectionMatrix> projectionMatrix;
        dmat4 matrix;
    };
    VSG_type_name(vsg::RelativeProjection);

    /// EllipsoidPerspective is a ProjectionMatrix that implements the gluPerspective model for setting the projection matrix,
    /// with automatic clamping of the near/far values to an ellipsoidModel, typically used for rendering whole earth models.
    class EllipsoidPerspective : public Inherit<ProjectionMatrix, EllipsoidPerspective>
    {
    public:
        EllipsoidPerspective() {}

        EllipsoidPerspective(ref_ptr<LookAt> la, ref_ptr<EllipsoidModel> em) :
            lookAt(la),
            ellipsoidModel(em)
        {
        }

        EllipsoidPerspective(ref_ptr<LookAt> la, ref_ptr<EllipsoidModel> em, double fov, double ar, double nfr, double hmh) :
            lookAt(la),
            ellipsoidModel(em),
            fieldOfViewY(fov),
            aspectRatio(ar),
            nearFarRatio(nfr),
            horizonMountainHeight(hmh)
        {
        }

        dmat4 transform() const override
        {
            //debug("camera eye : ", lookAt->eye, ", ", ellipsoidModel->convertECEFToLatLongAltitude(lookAt->eye));
            vsg::dvec3 v = lookAt->eye;
            vsg::dvec3 lv = vsg::normalize(lookAt->center - lookAt->eye);
            double R = ellipsoidModel->radiusEquator();
            double H = ellipsoidModel->convertECEFToLatLongAltitude(v).z;
            double D = R + H;

            double alpha = (D > R) ? std::acos(R / D) : 0.0;

            double beta_ratio = R / (R + horizonMountainHeight);
            double beta = beta_ratio < 1.0 ? std::acos(beta_ratio) : 0.0;

            double theta_ratio = -vsg::dot(lv, v) / (vsg::length(lv) * vsg::length(v));
            double theta = theta_ratio < 1.0 ? std::acos(theta_ratio) : 0.0;

            double l = R * (std::tan(alpha) + std::tan(beta));

            double farDistance = std::cos(theta + alpha - vsg::PI * 0.5) * l;
            double nearDistance = farDistance * nearFarRatio;
            //debug("H = ", H, ", l = ", l, ", theta = ", vsg::degrees(theta), ", fd = ", farDistance);

            return perspective(radians(fieldOfViewY), aspectRatio, nearDistance, farDistance);
        }

        void changeExtent(const VkExtent2D& prevExtent, const VkExtent2D& newExtent) override
        {
            double oldRatio = static_cast<double>(prevExtent.width) / static_cast<double>(prevExtent.height);
            double newRatio = static_cast<double>(newExtent.width) / static_cast<double>(newExtent.height);

            aspectRatio *= (newRatio / oldRatio);
        }

        ref_ptr<LookAt> lookAt;
        ref_ptr<EllipsoidModel> ellipsoidModel;
        double fieldOfViewY = 60.0;
        double aspectRatio = 1.0;
        double nearFarRatio = 0.0001;
        double horizonMountainHeight = 1000.0;

    public:
        void read(Input& input) override;
        void write(Output& output) const override;
    };
    VSG_type_name(vsg::EllipsoidPerspective);

} // namespace vsg

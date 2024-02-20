/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/Animation.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Options.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/MatrixTransform.h>

using namespace vsg;

AnimationSampler::AnimationSampler()
{
}

AnimationSampler::AnimationSampler(const AnimationSampler& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    name(rhs.name)
{
}

int AnimationSampler::compare(const Object& rhs_object) const
{
    int result = Visitor::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value(name, rhs.name);
}

void AnimationSampler::read(Input& input)
{
    Object::read(input);
    input.read("name", name);
}

void AnimationSampler::write(Output& output) const
{
    Object::write(output);
    output.write("name", name);
}

Animation::Animation()
{
}

Animation::Animation(const Animation& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    name(rhs.name),
    mode(rhs.mode),
    speed(rhs.speed),
    samplers(copyop(rhs.samplers)),
    _active(false),
    _startTime(rhs._startTime),
    _maxTime(rhs._maxTime)
{
}

int Animation::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(name, rhs.name)) != 0) return result;
    if ((result = compare_value(mode, rhs.mode)) != 0) return result;
    if ((result = compare_value(speed, rhs.speed)) != 0) return result;

    return compare_pointer_container(samplers, rhs.samplers);
}

void Animation::read(Input& input)
{
    Object::read(input);

    input.read("name", name);
    input.readValue<uint32_t>("mode", mode);
    input.read("speed", speed);
    input.readObjects("samplers", samplers);
}

void Animation::write(Output& output) const
{
    Object::write(output);

    output.write("name", name);
    output.writeValue<uint32_t>("mode", mode);
    output.write("speed", speed);
    output.writeObjects("samplers", samplers);
}

bool Animation::start(double simulationTime)
{
    _startTime = simulationTime;
    _maxTime = 0.0;

    if (samplers.empty())
    {
        _active = false;
        return false;
    }

    for (auto sampler : samplers)
    {
        _maxTime = std::max(_maxTime, sampler->maxTime());
    }

    _active = true;
    return _active;
}

bool Animation::update(double simulationTime)
{
    if (!_active) return false;

    // TODO: need to use delta since last time update...
    double time = (simulationTime - _startTime) * speed;
    if (mode == REPEAT)
    {
        time = std::fmod(time, _maxTime);
    }
    else if (mode == FORWARD_AND_BACK)
    {
        time = std::fmod(time, 2.0 * _maxTime);
        if (time > _maxTime) time = 2.0 * _maxTime - time;
    }
    else
    {
        if (time > _maxTime)
        {
            _active = false;
            return false;
        }
    }

    for (auto sampler : samplers)
    {
        sampler->update(time);
    }

    return true;
}


/// signal that the animation is to stop
bool Animation::stop(double /*simulationTime*/)
{
    _active = false;
    return false;
}

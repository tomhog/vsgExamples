#pragma once

#include <vsg/core/Object.h>

namespace vsgExamples
{
    // Base class for cross platform examples
    class ExampleBase : public vsg::Object
    {
    public:
        ExampleBase(){}

        // one time allocation of resource etc, returns non zero if error
        virtual int Init(int argc, char** argv) = 0;

        // update and render frame, if returns non zero then stop updating
        virtual int Update() = 0;

    protected:
        virtual ~ExampleBase(){}
    };
} // namespace vsgExamples
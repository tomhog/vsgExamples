#pragma once

#include <vsg/core/Object.h>

namespace vsgExamples
{
    // Base class for cross platform examples
    class ExampleBase : public vsg::Object
    {
    public:
		ExampleBase() {}

		// allocate resources, returns non zero on error
		virtual int init(int argc, char** argv) = 0;

        // update and render frame, if returns non zero then stop updating
        virtual int frame() = 0;

    protected:
        virtual ~ExampleBase(){}
    };
} // namespace vsgExamples

#include "DrawExample.h"

#include <iostream>

int main(int argc, char** argv)
{
	vsg::ref_ptr<DrawExample> drawexample(new DrawExample());
	int result = drawexample->init(argc, argv);

	if (result != 0) return result;

	while (drawexample->frame() == 0)
	{
	}

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}

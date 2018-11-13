#pragma once

#include <vsg/all.h>
#include <osg2vsg/ImageUtils.h>
#include <chrono>

#include "ExampleBase.h"

class DrawExample : public vsgExamples::ExampleBase
{
public:
	DrawExample();

	// allocate resources, returns non zero on error
	virtual int init(int argc, char** argv);

	// update and render frame, if returns non zero then stop updating
	virtual int frame();

protected:
	virtual ~DrawExample();

protected:

	vsg::ref_ptr<vsg::Viewer> _viewer;

	vsg::BufferDataList _uniformBufferData;

	vsg::ref_ptr<vsg::mat4Value> _projMatrix;
	vsg::ref_ptr<vsg::mat4Value> _viewMatrix;
	vsg::ref_ptr<vsg::mat4Value> _modelMatrix;

	uint32_t _width;
	uint32_t _height;

	std::chrono::steady_clock::time_point _startTime;
	float _time = 0.0f;

	int _numFrames = -1;
	bool _printFrameRate;
};


set(SOURCES vsgdraw.cpp)

add_executable(vsgdrawdesktop ${SOURCES})

target_link_libraries(vsgdrawdesktop vsg::vsg osg2vsg::osg2vsg)

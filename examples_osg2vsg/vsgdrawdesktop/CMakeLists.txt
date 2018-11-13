set(SOURCES 
    vsgdraw.cpp
    DrawExample.cpp
)

set(HEADERS 
    ExampleBase.h
    DrawExample.h
)

add_executable(vsgdrawdesktop ${SOURCES} ${HEADERS})

target_link_libraries(vsgdrawdesktop vsg::vsg osg2vsg::osg2vsg)

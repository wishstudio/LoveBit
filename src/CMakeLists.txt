add_subdirectory(qjson)

set(LoveBit_SRCS main.cpp SHA256.cpp)
set(LoveBit_RCCS CLFiles.qrc)

qt4_add_resources(LoveBit_RCC_SRCS ${LoveBit_RCCS})

add_executable(LoveBit ${LoveBit_SRCS} ${LoveBit_RCC_SRCS})

target_link_libraries(LoveBit qjson ${QT_LIBRARIES} ${OPENCL_LIBRARIES})

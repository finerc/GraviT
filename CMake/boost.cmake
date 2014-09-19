set(Boost_USE_STATIC_LIBS       OFF) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)


find_package(Boost 1.55.0 COMPONENTS system thread timer chrono)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()


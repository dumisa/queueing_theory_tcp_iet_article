check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/my-dumbbell-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME my-dumbbell
    SOURCE_FILES model/my-dumbbell.cc
                 helper/my-dumbbell-helper.cc
    HEADER_FILES model/my-dumbbell.h
                 helper/my-dumbbell-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libinternet}
                      ${libpoint-to-point}
                      ${libmobility}
                      ${libapplications}
                      ${libtraffic-control}
    TEST_SOURCES test/my-dumbbell-test-suite.cc
                 ${examples_as_tests_sources}
)


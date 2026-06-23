# CMake generated Testfile for 
# Source directory: /home/ling/WorkSpace/LogMind/tests
# Build directory: /home/ling/WorkSpace/LogMind/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[stress-test]=] "/home/ling/WorkSpace/LogMind/build/tests/logmind-stress-test")
set_tests_properties([=[stress-test]=] PROPERTIES  WORKING_DIRECTORY "/home/ling/WorkSpace/LogMind" _BACKTRACE_TRIPLES "/home/ling/WorkSpace/LogMind/tests/CMakeLists.txt;32;add_test;/home/ling/WorkSpace/LogMind/tests/CMakeLists.txt;0;")
add_test([=[smoke-test]=] "/home/ling/WorkSpace/LogMind/build/tests/logmind-smoke-test")
set_tests_properties([=[smoke-test]=] PROPERTIES  WORKING_DIRECTORY "/home/ling/WorkSpace/LogMind" _BACKTRACE_TRIPLES "/home/ling/WorkSpace/LogMind/tests/CMakeLists.txt;55;add_test;/home/ling/WorkSpace/LogMind/tests/CMakeLists.txt;0;")

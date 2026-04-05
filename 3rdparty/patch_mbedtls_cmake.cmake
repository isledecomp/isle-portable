file(READ "CMakeLists.txt" content)
string(REGEX REPLACE
    "cmake_minimum_required\\(VERSION [0-9]+\\.[0-9]+[0-9.]*\\)"
    "cmake_minimum_required(VERSION 3.10)"
    content "${content}"
)
file(WRITE "CMakeLists.txt" "${content}")

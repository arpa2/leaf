include_directories (${CMAKE_SOURCE_DIR}/src)
include_directories (${LillyDAP_INCLUDE_DIRS})

add_executable (messagelog
	messagelog.c
	)
target_link_libraries (messagelog
	leaf
	${LillyDAP_LIBRARIES}
	${Quick-DER_LIBRARIES}
	)

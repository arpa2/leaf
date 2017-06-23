include_directories(${LillyDAP_INCLUDE_DIRS})

add_library (leaf STATIC
	network.c
	region.c
	)


if(NOT TARGET glm::glm)
	# GLM: header-only math library (vector/matrix types, geometry utilities).
	CPMFindPackage(
		NAME              glm
		GITHUB_REPOSITORY g-truc/glm
		GIT_TAG           1.0.1
		VERSION           1.0.1
	)
endif()

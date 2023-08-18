# settings will be taken from .clang-format. .editorconfig is just for android studio & VSStudio if ever used
.PHONY: format

format:	
	find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.c" \) ! -path "*/lib*" \
		! -path "*/ALVR/*" 		\
		! -path "*gvr*" 		\
		! -path "*cardboard*" 	\
		! -path "*.cxx*" 		\
		-exec clang-format -i {} + -print

format-check:
	find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.c" \) ! -path "*/lib*" \
		! -path "*/ALVR/*" 		\
		! -path "*gvr*" 		\
		! -path "*cardboard*" 	\
		! -path "*.cxx*" 		\
		-exec clang-format --dry-run --Werror {} + -print
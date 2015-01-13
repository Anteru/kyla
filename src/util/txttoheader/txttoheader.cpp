#include <iostream>
#include <fstream>
#include <vector>

int main (int /*argc*/, char* argv[])
{
	// Argv
	// 1 : variable name
	// 2 : file
	// Output goes to stdout
	std::ifstream input (argv [2], std::ios::binary);
	std::vector<char> buffer {std::istreambuf_iterator<char> (input),
							  std::istreambuf_iterator<char> ()};

	std::cout << "const char " << argv [1] << "[] = {" << std::endl;

	int lineEntries = 0;
	for (const char c : buffer) {
		std::cout << "0x" << std::hex << (int)c << ", ";
		if (lineEntries++ == 19) {
			std::cout << '\n';
			lineEntries = 0;
		}
	}

	std::cout << "0" << "};\n\n";
}

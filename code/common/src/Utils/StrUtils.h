#pragma once

#include <sstream>
#include <memory>

namespace std {
#ifdef __ANDROID__
	template<typename T>
	string to_string(T value) {
		ostringstream os;
		os << value;
		return os.str();
	}
#endif

	inline string to_string(string &str) {
		return str;
	}

	inline string to_string(const char *str) {
		return str;
	}

}

template<typename ...Args>
std::string str_fmt(const std::string &format, Args ...args) {
	size_t size = (size_t)snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}


//code for string encription:
#if _WIN32
#pragma warning(push, 1) // disable all warnings for this block of code
#endif
namespace {

	//-------------------------------------------------------------//
	// "Malware related compile-time hacks with C++11" by LeFF   //
	//-------------------------------------------------------------//
	template<int X>
	struct EnsureCompileTime {
		enum : int {
			Value = X
		};
	};

	//Use Compile-Time as seed
#define Seed1 ((__TIME__[7] - '0') * 1  + (__TIME__[6] - '0') * 10  + \
              (__TIME__[4] - '0') * 60   + (__TIME__[3] - '0') * 600 + \
              (__TIME__[1] - '0') * 3600 + (__TIME__[0] - '0') * 36000)

	constexpr int LinearCongruentGenerator(int Rounds) {
		return 1013904223 +
			1664525 * ((Rounds > 0) ? LinearCongruentGenerator(Rounds - 1) : Seed1 & 0xFFFFFFFF);
	}

#define Random1() EnsureCompileTime<LinearCongruentGenerator(10)>::Value //10 Rounds
#define RandomNumber(Min, Max) (Min + (Random1() % (Max - Min + 1)))

	template<int... Pack>
	struct IndexList {};

	template<typename IndexList, int Right>
	struct Append;
	template<int... Left, int Right>
	struct Append<IndexList<Left...>, Right> {
		typedef IndexList<Left..., Right> Result;
	};

	template<int N>
	struct ConstructIndexList {
		typedef typename Append<typename ConstructIndexList<N - 1>::Result, N - 1>::Result Result;
	};
	template<>
	struct ConstructIndexList<0> {
		typedef IndexList<> Result;
	};

	const char XORKEY = static_cast<char>(RandomNumber(0, 0xFF));

	constexpr char EncryptCharacter(const char Character, int Index) {
		return Character ^ (XORKEY + Index);
	}

	template<typename IndexList>
	class CXorString;

	template<int... Index>
	class CXorString<IndexList<Index...> > {
	private:
		char Value[sizeof...(Index)+1];
	public:
		constexpr CXorString(const char *const String)
			: Value{ EncryptCharacter(String[Index], Index)... } {}

		char *decrypt() {
			for (int t = 0; t < sizeof...(Index); t++) {
				Value[t] = Value[t] ^ (XORKEY + t);
			}
			Value[sizeof...(Index)] = '\0';
			return Value;
		}

		char *get() {
			return Value;
		}
	};
}
#if _WIN32
#pragma warning(pop)
#endif

#define crypt(String) ( CXorString<ConstructIndexList<sizeof( String ) - 1>::Result>( String ).decrypt() )

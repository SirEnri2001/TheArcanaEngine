#pragma once

#include <iostream>

#define Warning(...) BasicWarning(__FILE__, "(", __LINE__, "): Warning: ", __VA_ARGS__)
#define Error(...) BasicError(__FILE__, "(", __LINE__, "): Error: ", __VA_ARGS__)
#define Log(...) BasicLog(__FILE__, "(", __LINE__, "): Log: ", __VA_ARGS__)

template<typename T>
inline void BasicLogMsg(std::ostream& os, T Msg) {
	os << Msg;
}

template<typename T, typename... Args>
inline void BasicLogMsg(std::ostream& os, T Msg, Args... args) {
	os << Msg;
	BasicLogMsg(os, args...);
}

template<typename T, typename... Args>
inline void BasicLog(T Msg, Args... args) {
	std::cout << "\u001b[32m";
	BasicLogMsg(std::cout, Msg, args...);
	std::cout << "\u001b[0m\n";
}

template<typename T, typename... Args>
inline void BasicWarning(T Msg, Args... args) {
	std::cout << "\u001b[30m\u001b[43m";
	BasicLogMsg(std::cout, Msg, args...);
	std::cout << "\u001b[0m\n";
}

template<typename T, typename... Args>
inline void BasicError(T Msg, Args... args) {
	std::cout << "\u001b[31m";
	BasicLogMsg(std::cout, Msg, args...);
	std::cout << "\u001b[0m\n";
}
#pragma once
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <thread>
inline unsigned checks=0;
inline void Check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
template<class F>void Until(F f){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!f()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Worker test timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}

#include <iostream>
#include "expand.h"

void test(const std::string& before, const std::string& after, int linum)
{
	using RT = std::pair<std::string, ExpandError>;
	RT res = expand(before);
	if (res.second == ExpandError::Ok) {
		if (res.first != after) {
			std::cout << "In line " << linum << ":\n"
					  << "original:\t" << before
					  << "\nwant:\t" << after
					  << "\ngot:\t" << res.first << "\n";
		} else {
			std::cout << "Line " << linum << " is ok\n";
		}
	} else {
		std::cout << "In line " << linum << ":\n"
				  << "syntax error\n";
	}
}

#define Test(before, after) test(before, after, __LINE__)

/*
int main()
{
	Test("python'123'", "python123");
	Test("python\"123\"", "python123");
	Test("python\'123$((1+2))\'", "python123$((1+2))");
	Test("python\"123$((1+2))\"", "python1233");
	Test("python$((1+2))", "python3");
	Test("python\\1", "python\\1");
	Test("python\\\\", "python\\\\");
	Test("python$(($((1+1))+1))", "python3");
	Test("python\\)", "python\\)");
	Test("python\"\\)\"", "python\\)");
	Test("python\'\\)\'", "python\\)");
	Test("echo $((1+2*3-4))", "echo 3");
	Test("(sleep; ping; python$((1+2)))", "(sleep; ping; python3)");
	Test("cd ~", "cd /home/shiletong");
	Test("cd ~/codes", "cd /home/shiletong/codes");
}
*/

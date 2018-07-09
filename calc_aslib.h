#ifndef CALC_ASLIB_H__
#define CALC_ASLIB_H__

struct CalcResult
{
private:
	
			
	union {
		double d;
		int i;
	} u;
public:
	enum CalcResultType {
		Int, Double, Error
	};
	const CalcResultType type;
	const bool error;
	CalcResult() : type(Error), error(true) {}
	CalcResult(int v, CalcResultType t) : type(t), error(false) {
		u.i = v;
	}
	CalcResult(double v, CalcResultType t) : type(t), error(false) {
		u.d = v;
	}

	int intResult() { return u.i; }
	double doubleResult() { return u.d; }
};

CalcResult calc(const std::string&);

#endif

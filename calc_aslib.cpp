// let '-' be a unary optor
// only this version is possible to be correct
// qufen zhengshu he fudian
#include <sstream>
#include <vector>
#include <string>
#include <utility>
#include <bitset>
#include <cassert>
#include <cctype>
#include <iterator>
#include <iostream>
#include <memory>
#include <cstdio>

#include "calc_aslib.h"

// Token Part
enum FaStatus
{
    S0_BEG, S0, S1, S2, S3, S4, S5, Se, INVALID
};

const int statusNum = 8;

enum TokenType
{
    INT, DOUBLE, LPAREN, RPAREN, OP, BEG, BAD
};

class Token
{
public:
    Token() = default;
    explicit Token( char op_ )
	{
	    u.op = op_;
	    type = OP;
	}

    explicit Token( double num_ )
	{
	    u.dnum = num_;
	    type = DOUBLE;
	}

    explicit Token( int num_ )
	{
	    u.inum = num_;
	    type = INT;
	}


    explicit Token( TokenType type_ )
	{
	    type = type_; 	// for paren
	}

    Token& setOp( char op_ )
	{
	    u.op = op_;
	    type = OP;
		return *this;
	}

    Token& setInt( int num_ )
	{
	    u.inum = num_;
	    type = INT;
		return *this;
	}

    Token& setDouble( double num_ )
	{
	    u.dnum = num_;
	    type = DOUBLE;
		return *this;
	}
    
    Token& setBad()
	{
	    type = BAD;
		return *this;
	}
    
    Token& setParen( TokenType type_ )
	{
	    type = type_;
		return *this;
	}

    double getDouble() const
	{
	    assert( type == DOUBLE );
	    return u.dnum;
	}
    
    int getInt() const
	{
	    assert( type == INT );
	    return u.inum;
	}

    char getOp() const
	{
	    assert( type == OP );
	    return u.op;
	}

    TokenType getType() const
	{
	    return type;
	}

    std::string toString() const
	{
	    std::ostringstream ss;
	    if( type == LPAREN ) {
			ss << "LeftParen";
	    } else if( type == RPAREN ) {
			ss << "RightParen";
	    } else if( type == OP ) {
			ss << "Operator" << u.op;
	    } else if( type == INT ) {
			ss << "Int(" << u.inum << ")"; 
	    } else if( type == DOUBLE ) {
			ss << "Double(" << u.dnum << ")";
	    } else {
			ss << "Invalid";
	    }
	    return ss.str();
	}
    
private:
    union {
		char op;
		double dnum;
		int inum;
    } u;
    
    TokenType type;
};

// Tree Part
enum class ValueType { Int, Double, Undecided };
class NodeBase
{
public:
    NodeBase() {
		restype = ValueType::Undecided;
    }
    virtual ~NodeBase() = default;
    bool isInt() const {
		return restype == ValueType::Int;
    }
    bool isDouble() const {
		return restype == ValueType::Double;
    }
    virtual void decideType() = 0;
    virtual int evalInt() const = 0;
    virtual double evalDouble() const = 0;


    void setType( ValueType t ) {
		restype = t;
    }

    ValueType getType() const {
		return restype;
    }
private:
    ValueType restype;
};

class BinaryNode : public NodeBase
{
public:
    BinaryNode( char op_, std::unique_ptr<NodeBase> l, std::unique_ptr<NodeBase> r )
		: op(op_), left(std::move(l)), right(std::move(r)) {
		assert( op == '+' || op == '-' || op == '*' || op == '/' );
    }

    void setLeft( std::unique_ptr<NodeBase> left_ ) {
		left = std::move(left_);
    }

    void setRight( std::unique_ptr<NodeBase> right_ ) {
		right = std::move(right_);
    }
    
    void decideType() override {
		left->decideType();
		right->decideType();
		if( left->isDouble() || right->isDouble() ) {
			setType( ValueType::Double );
		} else {
			setType( ValueType::Int );
		}
    }

    int evalInt() const override {
		assert( getType() == ValueType::Int );
		switch( op ) {
		case '+': return left->evalInt() + right->evalInt();
		case '-': return left->evalInt() - right->evalInt();
		case '*': return left->evalInt() * right->evalInt();
		case '/': return left->evalInt() / right->evalInt();
		default: assert( false );
		}
    }

    double evalDouble() const override {
		assert( getType() == ValueType::Double );
		switch( op ) {
		case '+': {
			unsigned l = !!(left->getType() == ValueType::Double);
			unsigned r = !!(right->getType() == ValueType::Double);
			switch( 2 * r + l ) {
			case 0: return left->evalInt() + right->evalInt();
			case 1: return left->evalDouble() + right->evalInt();
			case 2: return left->evalInt() + right->evalDouble();
			case 3: return left->evalDouble() + right->evalDouble();
			default: assert(false);
			}
		}
		case '-': {
			unsigned l = !!(left->getType() == ValueType::Double);
			unsigned r = !!(right->getType() == ValueType::Double);
			switch( 2 * r + l ) {
			case 0: return left->evalInt() - right->evalInt();
			case 1: return left->evalDouble() - right->evalInt();
			case 2: return left->evalInt() - right->evalDouble();
			case 3: return left->evalDouble() - right->evalDouble();

			default: assert(false); return 0;// make g++ happy
			}
		}
		case '*': {
			unsigned l = !!(left->getType() == ValueType::Double);
			unsigned r = !!(right->getType() == ValueType::Double);
			switch( 2 * r + l ) {
			case 0: return left->evalInt() * right->evalInt();
			case 1: return left->evalDouble() * right->evalInt();
			case 2: return left->evalInt() * right->evalDouble();
			case 3: return left->evalDouble() * right->evalDouble();

			default: assert(false);
			}
		}
		case '/': {
			unsigned l = !!(left->getType() == ValueType::Double);
			unsigned r = !!(right->getType() == ValueType::Double);
			switch( 2 * r + l ) {
			case 0: return left->evalInt() / right->evalInt();
			case 1: return left->evalDouble() / right->evalInt();
			case 2: return left->evalInt() / right->evalDouble();
			case 3: return left->evalDouble() / right->evalDouble();

			default: assert(false);
			} 
		}
		default: assert(false);
		}
    }    
private:
	char op;
    std::unique_ptr<NodeBase> left;
    std::unique_ptr<NodeBase> right;
};

class UnaryNode : public NodeBase
{
public:
    UnaryNode() : op('-') {
	
    }

    UnaryNode( std::unique_ptr<NodeBase> child_ ) : op('-'), child(std::move(child_)) {}

    void setChild( std::unique_ptr<NodeBase> p ) {
		child = std::move( p );
    }
    
    virtual void decideType() {
		child->decideType();
		setType( child->getType() );
    }

    int evalInt() const {
		// switch op
		assert( child->isInt() );
		return -child->evalInt();
    }

    double evalDouble() const {
		assert( child->isDouble() );
		return -child->evalDouble();
    }
private:
	char op;
    std::unique_ptr<NodeBase> child;
};

class LeafNode : public NodeBase
{
public:
    LeafNode( int i_ ) {
		u.i = i_;
		setType( ValueType::Int );
    }

    LeafNode( double d_ ) {
		u.d = d_;
		setType( ValueType::Double );
    }

    void decideType() {}
    int evalInt() const override {
		return u.i;
    }
    double evalDouble() const override {
		return u.d;
    }
private:
    union {
		int i;
		double d;
    } u;
};

namespace
{
// util funcs 
inline
bool isDigit1to9( char c )
{
    return std::isdigit( c ) && c !='0';
}

inline
void skipSpace( const std::string& text, std::size_t& pos )
{
    while( std::isspace( text[pos] ) )
		++pos;
}

template<typename OIter>
bool tokenize( const std::string& text, OIter out );
Token nextToken( const std::string& , std::size_t& );

std::unique_ptr<NodeBase> expr( const std::vector<Token>& , std::size_t& );
std::unique_ptr<NodeBase> term( const std::vector<Token>& , std::size_t& );
std::unique_ptr<NodeBase> factor( const std::vector<Token>& , std::size_t& );
std::unique_ptr<NodeBase> primary( const std::vector<Token>& , std::size_t& );

/*
void printTokens( const std::vector<Token>& tokens )
{
    for( auto t : tokens ) {
		std::cout << t.toString() << "  ";
    }
    std::cout << std::endl;
}

void fillAndPrint( const std::string& str )
{
    std::vector<Token> tokens;
    auto b = tokenize( str, std::back_inserter( tokens ) );
    if( !b ) {
		std::cout << "tokenize failed" << std::endl;
    } else 
		printTokens( tokens );
}

void testTokenizing()
{
    fillAndPrint( "1+2*3" );
    fillAndPrint( "   1+2*3  " );
    fillAndPrint( " 1+2*3  " );
    fillAndPrint( "(1 + 2.0) * 3  " );
    fillAndPrint( " (  1 + 2 - 33.45) / 66" );
    fillAndPrint( "( 3+ 4.8) * (-2.0 + 4)" );
    fillAndPrint( "-3" );
    fillAndPrint( "3--4))(())" );
    fillAndPrint( "-3- 4" );
    fillAndPrint( "5.5 + -1000.101" );
    fillAndPrint( "5.5 + - 1000.101" ); // will fail , as expected
    fillAndPrint( "((3+4)/2) - (0.5 *3)" );
    fillAndPrint( "-5" );
}

static int tcnt = 0;
void assertTrue( const std::string& text, double expect )
{
    std::vector<Token> tokens;
    if( !tokenize( text, std::back_inserter(tokens) ) ) {
		std::cout << "AssertTrue failed in the No." << tcnt << " test" << std::endl;
		std::cout << "Tokenize failed." << std::endl;
    } else {
		std::size_t pos = 0;
		auto root = expr( tokens, pos );
	
		if( !root ) {
			std::cout << "AssertTrue failed in the No." << tcnt << " test" << std::endl;
			std::cout << "Parse failed" << std::endl;
		} else {
	    
			if( pos < tokens.size() ) {
				std::cout << "AssertTrue failed in the No." << tcnt << " test" << std::endl;
				std::cout << "Expression with invalid suffix" << std::endl;
			} else {
				root->decideType();
				double actual;
				if( root->isDouble() )
					actual = root->evalDouble();
				else if( root->isInt() )
					actual = root->evalInt();
				else
					assert(false);
				if( actual != expect ) {
					std::cout << "AssertTrue failed in the No." << tcnt << " test" << std::endl;
					std::cout << "Expect: " << expect << "  Actual: " << actual << std::endl;
				}
			}

		}
    }
    ++tcnt;
}
*/
}

CalcResult calc(const std::string& str)
{
	std::vector<Token> toks;
	if (!tokenize(str, std::back_inserter(toks))) {
		return {};
	}

	std::size_t pos = 0;
	auto res = expr(toks, pos);
	if (pos != toks.size() || !res) {
		return {};
	}

	res->decideType();    
    if (res->isInt()) {
		return {res->evalInt(), CalcResult::Int};
    } else if (res->isDouble()) {
		return {res->evalInt(), CalcResult::Double};
    } else {
		return {};
    }
}


namespace {
std::unique_ptr<NodeBase> expr( const std::vector<Token>& tokens, std::size_t& pos )
{
    if( pos == tokens.size() ) {
		return nullptr;
    }

    std::unique_ptr<NodeBase> left;
    if( !(left = term( tokens, pos )) ) {
		return nullptr;
    }

    while( true ) {
		if( pos == tokens.size() ) {
			return left;
		}

		if( tokens[pos].getType() == OP ) {
			char op = tokens[pos++].getOp();
			if( op == '+' || op == '-' ) {
				auto right = term( tokens, pos );
				if( !right ) {
					return nullptr;
				}
				auto root = std::make_unique<BinaryNode>( op, std::move(left),
														  std::move(right) );	    
				left = std::move(root);

			} else {
				assert( false ); // bakana
				// return false;
			}
		} else {
			return left;
		}
    }
}

std::unique_ptr<NodeBase> term( const std::vector<Token>& tokens, std::size_t& pos )
{
    if( pos == tokens.size() ) {
		return nullptr;
    }

    std::unique_ptr<NodeBase> left;
    if( !(left = factor( tokens, pos )) ) {
		return nullptr;
    }

    while( true ) {
		if( pos == tokens.size() ) {
			return left;
		}
	
		if( tokens[pos].getType() == OP ) {
			char op = tokens[pos].getOp(); // don't ++
			if( op == '*' || op == '/' ) {
				++pos;
				auto right = factor( tokens, pos );
				if( !right ) {
					return nullptr;
				}

				auto root = std::make_unique<BinaryNode>( op, std::move(left),
														  std::move(right) );
				left = std::move(root);


			} else {
				// + or - in FOLLOW(factor)
				return left;
			}
		} else {
			return left;
		}
	
    }    
}

std::unique_ptr<NodeBase> factor( const std::vector<Token>& tokens, std::size_t& pos )
{
    if( pos == tokens.size() ) {
		return nullptr;
    }
    
    Token tok = tokens[pos];
    if( tok.getType() == OP && tok.getOp() == '-' ) {
		++pos;
		auto subroot = primary( tokens, pos );
		if( !subroot ) {
			return nullptr;
		}
	
		return std::make_unique<UnaryNode>( std::move(subroot) );
    } else {
		return primary( tokens, pos );
    }
}

std::unique_ptr<NodeBase> primary( const std::vector<Token>& tokens, std::size_t& pos )
{
    if( pos == tokens.size() ) {
		return nullptr;
    }
    
    Token tok = tokens[pos++];
    if( tok.getType() == INT ) {
		return std::make_unique<LeafNode>( tok.getInt() );
    } else if( tok.getType() == DOUBLE ) {
		return std::make_unique<LeafNode>( tok.getDouble() );
    } else if( tok.getType() == LPAREN ) {
		auto subroot = expr( tokens, pos );
		if( !subroot ) {
			return nullptr;
		}

		if( pos == tokens.size() ) {
			return nullptr;	// 3 + (
		}
	
		Token tail = tokens[pos++];
		if( tail.getType() == RPAREN ) {
			return subroot;
		} else {
			return nullptr;
		}
    } else {
		return nullptr;
    }
}

template<typename OIter>
bool tokenize( const std::string& text, OIter out )
{
    TokenType type = BEG;
    std::size_t pos = 0;

    skipSpace( text, pos );
    while( pos < text.size() ) {
	
		Token tmp = nextToken( text, pos );
		type = tmp.getType();
		if( type == BAD ) { // maybe improved
			return false;
		}

		*out = tmp;
		++out;

		skipSpace( text, pos );
    }

    return true;
}

Token nextToken( const std::string& text, std::size_t& pos )
{
    Token res;
    FaStatus status = S0_BEG;
    std::vector<FaStatus> stack;
    std::string lexeme;
    char curr;
    std::bitset<statusNum> accepted( 0x5C );
    for(; pos < text.size() && status != Se; ++pos ) {
		curr = text[pos];
		lexeme.push_back( curr );
		if( accepted[status] ) {
			stack.clear();
		}
		stack.push_back( status );
		switch( status ) {
		case S0:
		case S0_BEG:
			if( curr == '+' || curr == '-' || curr == '*'
				|| curr == '/' || curr == '(' || curr == ')' ) {
				status = S1;
			} else if( isDigit1to9( curr ) ) {
				status = S3;
			} else if( curr == '0' ) {
				status = S2;
			} else {
				status = Se;
			}
			break;
		case S1:
			status = Se;
			break;
		case S2:
			if( curr == '.' ) {
				status = S4;
			} else {
				status = Se;
			}
			break;
		case S3:
			if( std::isdigit( curr ) ) {
				status = S3;
			} else if( curr == '.' ){
				status = S4;
			} else {
				status = Se;
			}
			break;
		case S4:
			if( std::isdigit( curr ) ) {
				status = S5;
			} else {
				status = Se;
			}
			break;
		case S5:
			if( std::isdigit( curr ) ) {
				status = S5;
			} else {
				status = Se;
			}
			break;
		default:
			assert( false );
		}
    } // end for

    while( status != S0_BEG && !accepted[status] ) {
		status = stack.back();
		stack.pop_back();
		lexeme.pop_back();
		--pos;
    }

    if( accepted[status] ) {
		if( status == S1 ) {
			assert( lexeme.size() == 1 );
			if( lexeme[0] == '(' ) {
				res.setParen( LPAREN );
			} else if( lexeme[0] == ')' ) {
				res.setParen( RPAREN );
			} else {
				res.setOp( lexeme[0] );
			}
		} else {
			// number
			if( status == S2 || status == S3 ) {
				// integer
				res.setInt( std::stoi( lexeme ) );		
			} else {
				// double
				assert( status == S5 );
				res.setDouble( std::stod( lexeme ) );
			}
		}
    } else {
		res.setBad();
    }

    return res;
}
}

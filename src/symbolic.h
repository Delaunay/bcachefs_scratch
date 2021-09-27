#ifndef PROJECT_TEST_SRC_SYMBOLIC_HEADER
#define PROJECT_TEST_SRC_SYMBOLIC_HEADER

#include <unordered_map>
#include <ostream>
#include <string>
#include <memory>

namespace sym
{

class ABSExpr;
using Expr = std::shared_ptr<ABSExpr>;
using Context = std::unordered_map<std::string, Expr>;

class ABSExpr
{
public:
    virtual double full_eval(const Context&) = 0;
    virtual Expr partial_eval(const Context&) = 0;
    virtual Expr derivate(const std::string&) = 0;
    virtual std::ostream& gen(std::ostream&) = 0;

    virtual ~ABSExpr(){}
};


/*!
 * \brief The Placeholder class documentation
 * \param name: name of thep placeholder
 */
class Placeholder: public ABSExpr
{
public:
    Placeholder(const std::string& name) noexcept:
        _name(name)
    {}

    double full_eval(const Context&) override;
    Expr partial_eval(const Context&) override;
    std::ostream& gen(std::ostream&) override;
    Expr derivate(const std::string&) override;

    static Expr make(const std::string& name){
        return std::make_shared<Placeholder>(name);
    }
private:
    std::string _name;
};

class Scalar: public ABSExpr
{
public:
    Scalar(double v) noexcept:
        _value(v)
    {}

    double full_eval(const Context&) override;
    Expr partial_eval(const Context&) override;
    std::ostream& gen(std::ostream&) override;
    Expr derivate(const std::string&) override;

    static Expr make(double v){
        return std::make_shared<Scalar>(v);
    }
private:
    double _value;
};


/*!
 * \brief The Add class
 * \param Expr a:
 */
class Add: public ABSExpr
{
public:
    Add(Expr a, Expr b) noexcept:
        _lhs(a), _rhs(b)
    {}

    double full_eval(const Context&) override;
    Expr partial_eval(const Context&) override;
    std::ostream& gen(std::ostream&) override;
    Expr derivate(const std::string&) override;

    static Expr make(Expr a, Expr b){
        return std::make_shared<Add>(a, b);
    }

private:
    Expr _lhs;
    Expr _rhs;
};

class Mult: public ABSExpr
{
public:
    Mult( Expr a,  Expr b) noexcept:
        _lhs(a), _rhs(b)
    {}

    double full_eval(const Context&) override;
    Expr partial_eval(const Context&) override;
    std::ostream& gen(std::ostream&) override;
    Expr derivate(const std::string&) override;

    static Expr make(Expr a, Expr b){
        return std::make_shared<Mult>(a, b);
    }

private:
    Expr _lhs;
    Expr _rhs;
};


Expr make_var(const std::string& name);
Expr make_val(double v);
Expr mult(Expr l, Expr r);
Expr add(Expr l, Expr r);
void print(Expr f);

}

#endif

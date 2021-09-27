#include "symbolic.h"
#include <iostream>

namespace sym{

double Placeholder::full_eval(const Context& c)     { return c.at(_name)->full_eval(c); }
Expr Placeholder::partial_eval(const Context& c)    {
    try{
        return c.at(_name);
    } catch (std::out_of_range const&) {
        return Placeholder::make(_name);
        // return std::weak_ptr(?);
    }
}
std::ostream& Placeholder::gen(std::ostream& out)   { return out << _name;}
Expr Placeholder::derivate(const std::string& n)    { return n == _name ? Scalar::make(1): Scalar::make(0); }

double Scalar::full_eval(const Context&)        {   return _value; }
Expr Scalar::partial_eval(const Context&)       {   return Scalar::make(_value); }
std::ostream& Scalar::gen(std::ostream& out)    {   return out << _value; }
Expr Scalar::derivate(const std::string&)       {   return Scalar::make(0); }

double Add::full_eval(const Context& c)     {   return _lhs->full_eval(c) + _rhs->full_eval(c); }
Expr Add::partial_eval(const Context& c)    {   return Add::make(_lhs->partial_eval(c), _rhs->partial_eval(c));}
std::ostream& Add::gen(std::ostream& out)   {   out << "("; _lhs->gen(out) << " + "; _rhs->gen(out) << ")"; return out;}
Expr Add::derivate(const std::string& c)    {   return Add::make(_lhs->derivate(c), _rhs->derivate(c));}

double Mult::full_eval(const Context& c)    {   return _lhs->full_eval(c) * _rhs->full_eval(c); }
Expr Mult::partial_eval(const Context& c)   {   return Mult::make(_lhs->partial_eval(c), _rhs->partial_eval(c));}
std::ostream& Mult::gen(std::ostream& out)  {   out << "("; _lhs->gen(out) << " * "; _rhs->gen(out) << ")"; return out;}

Expr Mult::derivate(const std::string& c)   {
    auto a = Mult::make(_lhs->derivate(c), _rhs);
    auto b = Mult::make(_lhs, _rhs->derivate(c));
    return Add::make(a, b);
}

Expr make_var(const std::string& name)  {   return Placeholder::make(name);   }
Expr make_val(double v)                 {   return Scalar::make(v);   }
Expr mult(Expr l , Expr r)              {   return Mult::make(l, r);  }
Expr add(Expr l , Expr r)               {   return Add::make(l, r);   }

void print(Expr f) { f->gen(std::cout) << std::endl; }
}

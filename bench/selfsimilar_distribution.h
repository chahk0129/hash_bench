#ifndef SELFSIMILAR_DISTRIBUTION_H_ 
#define SELFSIMILAR_DISTRIBUTION_H_

#include <cassert>
#include <cmath>
#include <limits>
#include <random>

template <typename _IntType = int>
class selfsimilar_int_distribution{
    static_assert(std::is_integral<_IntType>::value, "Template argument not an integral type.");

    public:
        typedef _IntType result_type;
	struct param_type{
	    typedef selfsimilar_int_distribution<_IntType> distribution_type;
	    explicit param_type(_IntType __a = 0, _IntType __b = std::numeric_limits<_IntType>::max(), double __skew = 0.2)
		: _M_a(__a), _M_b(__b), _M_skew(__skew){
	        assert(_M_a <= _M_b && _M_skew > 0.0 && _M_skew < 1.0);
	    }

	    _IntType a() const { return _M_a; }
	    _IntType b() const { return _M_b; }
	    double skew() const { return _M_skew; }

	    friend bool operator==(const param_type& __p1, const param_type& __p2){
		return __p1._M_a == __p2._M_a
		    && __p1._M_b == __p2._M_b
		    && __p1._M_skew == __p2._M_skew;
	    }

	    private:
	        _IntType _M_a;
	        _IntType _M_b;
	        double _M_skew;
	};

    public:
	explicit selfsimilar_int_distribution(_IntType __a = _IntType(0), _IntType __b = _IntType(1), double __skew = 0.2)
	    : _M_param(__a, __b, __skew) { }
	explicit selfsimilar_int_distribution(const param_type& __p): _M_param(__p) { }
	
	void reset() { }
	_IntType a() const { return _M_param.a(); }
	_IntType b() const { return _M_param.b(); }
	double skew() const { return _M_param.skew(); }
	param_type param() const { return _M_param; }
	void param(const param_type& __param) { _M_param = __param; }
	_IntType min() const { return this->a(); }
	_IntType max() const { return this->b(); }


	template <typename _UniformRandomNumberGenerator>
	_IntType operator()(_UniformRandomNumberGenerator& __urng){
	    return this->operator()(__urng, _M_param);
	}

	template <typename _UniformRandomNumberGenerator>
	_IntType operator()(_UniformRandomNumberGenerator& __urng, const param_type& __p){
	    double u = std::generate_canonical<double, std::numeric_limits<double>::digits, _UniformRandomNumberGenerator>(__urng);
	    unsigned long N = __p.b() - __p.a() + 1;
	    return __p.a() + (N * std::pow(u, std::log(__p.skew()) / std::log(1.0 - __p.skew())));
	}

	friend bool operator==(const selfsimilar_int_distribution& __d1, const selfsimilar_int_distribution& __d2){
	    return __d1._M_param == __d2._M_param;
	}

    private:
	param_type _M_param;
};

#endif



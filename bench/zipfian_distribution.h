#ifndef ZIPFIAN_DISTRIBUTION_H_
#define ZIPFIAN_DISTRIBUTION_H_

#include <cassert>
#include <cmath>
#include <limits>
#include <random>

template <typename _IntType = int>
class zipfian_int_distribution{
    static_assert(std::is_integral<_IntType>::value, "Template argument not an integral type.");

    public:
        typedef _IntType result_type;
	struct param_type{
	    typedef zipfian_int_distribution<_IntType> distribution_type;
	    explicit param_type(_IntType __a = 0, _IntType __b = std::numeric_limits<_IntType>::max(), double __theta = 0.99)
		: _M_a(__a), _M_b(__b), _M_theta(__theta),
		  _M_zeta(zeta(_M_b - _M_a + 1, __theta)), _M_zeta2theta(zeta(2, __theta)){
	        assert(_M_a <= _M_b && _M_theta > 0.0 && _M_theta < 1.0);
	    }

	    explicit param_type(_IntType __a, _IntType __b, double __theta, double __zeta)
		: _M_a(__a), _M_b(__b), _M_theta(__theta), _M_zeta(__zeta),
		  _M_zeta2theta(zeta(2, __theta)){
		__glibcxx_assert(_M_a <= _M_b && _M_theta > 0.0 && _M_theta < 1.0);
	    }

	    _IntType a() const { return _M_a; }
	    _IntType b() const { return _M_b; }
	    double theta() const { return _M_theta; }
	    double zeta() const { return _M_zeta; }
	    double zeta2theta() const { return _M_zeta2theta; }
	    friend bool operator==(const param_type& __p1, const param_type& __p2){
		return __p1._M_a == __p2._M_a
		    && __p1._M_b == __p2._M_b
		    && __p1._M_theta == __p2._M_theta
		    && __p1._M_zeta == __p2._M_zeta
		    && __p1._M_zeta2theta == __p2._M_zeta2theta;
	    }

	    private:
	        _IntType _M_a;
	        _IntType _M_b;
	        double _M_theta;
	        double _M_zeta;
	        double _M_zeta2theta;

		double zeta(unsigned long __n, double __theta){
		    double ans = 0.0;
		    for(unsigned long i=1; i<=__n; ++i){
			ans += std::pow(1.0/i, __theta);
		    }
		    return ans;
		}
	};

	explicit zipfian_int_distribution(_IntType __a = _IntType(0), _IntType __b = _IntType(1), double __theta = 0.99)
	    : _M_param(__a, __b, __theta) { }
	explicit zipfian_int_distribution(const param_type& __p): _M_param(__p) { }
	
	void reset() { }
	_IntType a() const { return _M_param.a(); }
	_IntType b() const { return _M_param.b(); }
	double theta() const { return _M_param.theta(); }
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
	    double alpha = 1 / (1 - __p.theta());
	    double eta = (1 - std::pow(2.0 / (__p.b() - __p.a() + 1), 1 - __p.theta())) / (1 - __p.zeta2theta() / __p.zeta());

	    double u = std::generate_canonical<double, std::numeric_limits<double>::digits, _UniformRandomNumberGenerator>(__urng);

	    double uz = u * __p.zeta();
	    if(uz < 1.0)
		return __p.a();
	    if(uz < 1.0 + std::pow(0.5, __p.theta()))
		return __p.a() + 1;

	    return __p.a() + ((__p.b() - __p.a() + 1) * std::pow(eta * u - eta+1, alpha));
	}

	friend bool operator==(const zipfian_int_distribution& __d1, const zipfian_int_distribution& __d2){
	    return __d1._M_param == __d2._M_param;
	}

    private:
	param_type _M_param;
};

#endif



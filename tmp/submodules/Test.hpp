#pragma once

namespace v4d::modules {
	class Test {
	public: 
	V4D_DEFINE_SUBMODULE( SUBMODULE_TYPE_TEST | 1 )
	
	protected:
	
		int val;
		
	public:
		
		Test(int a) : val(a) {}
		virtual ~Test(){}
		
		virtual int Run() = 0;
	};
}

class TestSubmodule : public v4d::modules::Test {
public:
	using v4d::modules::Test::Test;
	int Run() override {
		return val * 2 + 11;
	}
};

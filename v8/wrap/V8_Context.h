/*
 *
 *  Created on: 2016年1月23日
 *      Author: zhangyalei
 */

#ifndef V8_CONTEXT_H_
#define V8_CONTEXT_H_

#include <string>
#include <map>
#include "include/v8.h"
#include "V8_Convert.h"

namespace v8_wrap {

class module;

template<typename T>
class class_;

/// V8 isolate and context wrapper
class context
{
public:
	/// Create context with optional existing v8::Isolate
	explicit context(v8::Isolate* isolate = nullptr);
	~context();

	/// V8 isolate associated with this context
	v8::Isolate* isolate() { return isolate_; }

	/// Library search path
	std::string const& lib_path() const { return lib_path_; }

	/// Set new library search path
	void set_lib_path(std::string const& lib_path) { lib_path_ = lib_path; }

	/// Run script file, returns script result
	/// or empty handle on failure, use v8::TryCatch around it to find out why.
	/// Must be invoked in a v8::HandleScope
	v8::Handle<v8::Value> run_file(std::string const& filename);

	/// The same as run_file but uses string as the script source
	v8::Handle<v8::Value> run_script(std::string const& source, std::string const& filename = "");

	/// Set a V8 value in the context global object with specified name
	context& set(char const* name, v8::Handle<v8::Value> value);

	/// Set module to the context global object
	context& set(char const *name, module& m);

	/// Set class to the context global object
	template<typename T>
	context& set(char const* name, class_<T>& cl)
	{
		v8::HandleScope scope(isolate_);
		cl.class_function_template()->SetClassName(v8_wrap::to_v8(isolate_, name));
		return set(name, cl.js_function_template()->GetFunction());
	}

private:
	bool own_isolate_;
	v8::Isolate* isolate_;
	v8::Persistent<v8::Context> impl_;

	struct dynamic_module;
	using dynamic_modules = std::map<std::string, dynamic_module>;

	static void load_module(v8::FunctionCallbackInfo<v8::Value> const& args);
	static void run_file(v8::FunctionCallbackInfo<v8::Value> const& args);

	dynamic_modules modules_;
	std::string lib_path_;
};

} // namespace v8_wrap

#endif // V8_CONTEXT_H_

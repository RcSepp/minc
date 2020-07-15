#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "minc_api.h"
#include "minc_pkgmgr.h"
#include "json.h"

#define USE_MINC_PACKAGES
#define USE_BINARY_PACKAGES
#define USE_PYTHON_PACKAGES
#define USE_NODE_PACKAGES

#ifdef USE_PYTHON_PACKAGES
#define PY_SSIZE_T_CLEAN
#include <python3.7/Python.h>
#endif

#ifdef USE_NODE_PACKAGES
#include <node.h>
#define NODE_WANT_INTERNALS true
#include "tracing/trace_event.h"

struct NodeContext
{
	//TODO: This will hang in node::LoadEnvironment() when trying to load more than one file
	//TODO: This uses highly volatile internal API's and will likely be incompatible with any version but Node.js v10.x

	static const int DEFAULT_THREAD_POOL_SIZE = 4;

	struct NodeIsolateContext
	{
		const v8::Locker locker;
		const v8::Isolate::Scope isolate_scope;
		const v8::HandleScope h_scope;
		const v8::Local<v8::Context> context;
		const v8::Context::Scope context_scope;
		node::IsolateData* isolate_data;

		NodeIsolateContext(node::NodePlatform* v8_platform, v8::Isolate* isolate) :
			locker(v8::Locker(isolate)),
			isolate_scope(v8::Isolate::Scope(isolate)),
			h_scope(v8::HandleScope(isolate)),
			context(v8::Context::New(isolate)),
			context_scope(v8::Context::Scope(context))
		{
#if NODE_MAJOR_VERSION == 8
			isolate_data = node::CreateIsolateData(isolate, uv_default_loop());
#elif NODE_MAJOR_VERSION > 8
			isolate_data = node::CreateIsolateData(isolate, uv_default_loop(), v8_platform);
#endif
		}

		~NodeIsolateContext()
		{
			node::FreeIsolateData(isolate_data);
		}
	};

	node::NodePlatform* v8_platform;
	v8::Isolate* isolate;
	NodeIsolateContext* isolateContext;
	std::vector<node::Environment*> environments;

	NodeContext() : v8_platform(nullptr), isolate(nullptr), isolateContext(nullptr) {}
	~NodeContext()
	{
		free();
	}
	void free()
	{
		// for (node::Environment* environment: environments)
		// 	node::FreeEnvironment(environment);
		// environments.clear();

		if (isolateContext != nullptr)
		{
			delete isolateContext;
			isolateContext = nullptr;
		}

		if (isolate != nullptr)
		{
			isolate->Dispose();
			isolate = nullptr;
		}

		if (v8_platform != nullptr)
		{
			v8::V8::Dispose();
			v8::V8::ShutdownPlatform();
			v8_platform->Shutdown();
			delete v8_platform;
			v8_platform = nullptr;
		}
	}
	void Run(int argc, const char * argv[])
	{
		if (v8_platform == nullptr)
		{
#if NODE_MAJOR_VERSION > 8
			auto tracing_agent = new node::tracing::Agent();
			node::tracing::TraceEventHelper::SetAgent(tracing_agent);
#endif
#if NODE_MAJOR_VERSION == 8
			v8_platform = new node::NodePlatform(DEFAULT_THREAD_POOL_SIZE, uv_default_loop(), nullptr);
#elif NODE_MAJOR_VERSION > 8
			v8_platform = new node::NodePlatform(DEFAULT_THREAD_POOL_SIZE, tracing_agent->GetTracingController());
			node::tracing::TraceEventHelper::SetAgent(tracing_agent);
#endif
			v8::V8::InitializePlatform(v8_platform);
			v8::V8::Initialize();
		}

		if (isolate == nullptr)
		{
			v8::Isolate::CreateParams params;
			params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
			isolate = v8::Isolate::New(params);
		}

		if (isolateContext == nullptr)
		{
			isolateContext = new NodeIsolateContext(v8_platform, isolate);
		}

		int exec_argc;
		const char ** exec_argv;
		node::Init(&argc, argv, &exec_argc, &exec_argv);
		node::Environment* environment = node::CreateEnvironment(isolateContext->isolate_data, isolateContext->context, argc, argv, exec_argc, exec_argv);
		node::LoadEnvironment(environment);
		environments.push_back(environment);
	}
} NODE_CONTEXT;

#endif

bool hasSubdir(const char* path, const char* subdir)
{
	DIR* dir = opendir(path);
	for (dirent* entry = readdir(dir); entry != nullptr; entry = readdir(dir))
		if(entry->d_type == DT_DIR && strcmp(entry->d_name, subdir) == 0)
		{
			closedir(dir);
			return true;
		}
	closedir(dir);
	return false;
}

MincPackage* MincPackageManager::discoverPackage(std::string pkgName) const
{
	if (pkgName.empty())
		return nullptr; // Package not found

	// Search registered packages
	auto pkg = packages.find(pkgName);
	if (pkg != packages.end())
		return pkg->second; // Package found

	// Split package name into list of sub-packages
	std::stringstream pkgNameStream(pkgName);
	std::string subpkgName;
	std::vector<std::string> subpkgList;
	while (std::getline(pkgNameStream, subpkgName, PKG_PATH_SEPARATOR))
		subpkgList.push_back(subpkgName);

	auto discover = [&](std::string pkgDir) -> MincPackage*
	{
		if (pkgDir.back() != '/' && pkgDir.back() != '\\')
#ifdef _WIN32
			pkgDir.push_back('\\');
#else
			pkgDir.push_back('/');
#endif

		for (std::string subpkgName: subpkgList) // Traverse sub-packages of pkgName
			if(hasSubdir(pkgDir.c_str(), subpkgName.c_str())) // If a sub-directory named subpkgName exists in pkgDir
			{
				// Append subpkgName to pkgDir
				pkgDir += subpkgName;
#ifdef _WIN32
				pkgDir.push_back('\\');
#else
				pkgDir.push_back('/');
#endif

				std::string pkgPath;

				if (std::filesystem::exists(pkgPath = pkgDir + "minc_pkg.json")) // If a package configuration file exists for this sub-package, ...
				{
					Json::Value json;
					std::ifstream file(pkgPath.c_str());
					if (file)
					{
						bool success = Json::parse(file, &json);
						file.close();
						if (!success)
							raiseCompileError(("error parsing package configuration " + pkgPath).c_str());
					}

					auto libs = json.lst.find("libs");
					if (libs != json.lst.end())
					{
						// Load required libraries
						for (const Json::Value& required_lib: libs->second.arr)
							if (!required_lib.str.empty())
							{
								//std::cout << "loading required library " << required_lib.str << "\n";
								auto pkgHandle = dlopen(required_lib.str.c_str(), RTLD_NOW | RTLD_GLOBAL);
								if (pkgHandle == nullptr)
								{
									char *error = dlerror();
									if (error != nullptr)
										raiseCompileError(("error loading library " + required_lib.str + ": " + error).c_str());
									else
										raiseCompileError(("unable to load library " + required_lib.str).c_str());
								}
							}
					}
				}

#ifdef USE_MINC_PACKAGES
				if (std::filesystem::exists(pkgPath = pkgDir + subpkgName + ".minc")) // If a Python package library exists for this sub-package, ...
				{
					MincBlockExpr* pkgBlock = parseCFile(pkgPath.c_str());
					MINC_PACKAGE_MANAGER().import(pkgBlock);
					codegenExpr((MincExpr*)pkgBlock, nullptr);
				} else
#endif

#ifdef USE_BINARY_PACKAGES
				if (std::filesystem::exists(pkgPath = pkgDir + subpkgName + ".so")) // If a binary package library exists for this sub-package, ...
				{
#ifdef BINARY_PKG_REQUIREMENTS
					// Load required libraries
					const std::string required_libs[] = BINARY_PKG_REQUIREMENTS;
					for (const std::string& required_lib: required_libs)
					{
						//std::cout << "loading required library " << required_lib << "\n";
						auto pkgHandle = dlopen(required_lib.c_str(), RTLD_NOW | RTLD_GLOBAL);
						if (pkgHandle == nullptr)
						{
							char *error = dlerror();
							if (error != nullptr)
								raiseCompileError(("error loading library " + required_lib + ": " + error).c_str());
							else
								raiseCompileError(("unable to load library " + required_lib).c_str());
						}
					}
#endif

					// Load package library
					// Packages will be registed with the package manager during library initialization
					auto pkgHandle = dlopen(pkgPath.c_str(), RTLD_NOW);
					if (pkgHandle == nullptr)
					{
						char *error = dlerror();
						if (error != nullptr)
							raiseCompileError(("error loading package " + pkgPath + ": " + error).c_str());
						else
							raiseCompileError(("unable to load package " + pkgPath).c_str());
					}
				} else
#endif

#ifdef USE_PYTHON_PACKAGES
				if (std::filesystem::exists(pkgPath = pkgDir + subpkgName + ".py")) // If a Python package library exists for this sub-package, ...
				{
					if (!Py_IsInitialized())
						Py_Initialize();

					FILE* file = fopen(pkgPath.c_str(), "rb");
					PyRun_SimpleFileEx(file, pkgPath.c_str(), 1);
				} else
#endif

#ifdef USE_NODE_PACKAGES
				if (std::filesystem::exists(pkgPath = pkgDir + subpkgName + ".js")) // If a Node.js package library exists for this sub-package, ...
				{
					char *args = new char[pkgPath.size() + 2];
					args[0] = '\0';
					strcpy(args + 1, pkgPath.c_str());
					char* argv[] = {args + 0, args + 1};
					NODE_CONTEXT.Run(2, const_cast<const char**>(argv));
					delete[] args;
				} else
#endif

				// If no package library exists for this sub-package, ...
				{
					continue;
				}

				// Search registered packages again to discover newly added packages
				pkg = packages.find(pkgName);
				if (pkg != packages.end())
					return pkg->second; // Package found

				//TODO: Raise warning that subpkgName was not defined in pkgPath
			}

		return nullptr; // Package not found
	};

	if (subpkgList[0].empty())
	{
		pkgName = pkgName.substr(1);
		subpkgList.erase(subpkgList.begin());
		return discover(std::filesystem::current_path());
	}
	else
	{
		MincPackage* pkg;
		for (std::string pkgSearchPath: pkgSearchPaths)
			if ((pkg = discover(pkgSearchPath)) != nullptr)
				return pkg;
		return nullptr; // Package not found
	}
}

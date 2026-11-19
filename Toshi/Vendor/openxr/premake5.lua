project "OpenXR"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	exceptionhandling "On"

	includedirs
	{
		"include",
		"src",
		"src/common",
		"src/loader",
		"src/external/jsoncpp/include"
	}

	files
	{
		"include/openxr/*.h",
		"src/xr_generated_dispatch_table_core.h",
		"src/xr_generated_dispatch_table_core.c",
		"src/common/*.h",
		"src/common/*.hpp",
		"src/common/filesystem_utils.cpp",
		"src/common/object_info.cpp",
		"src/loader/*.hpp",
		"src/loader/*.cpp",
		"src/external/jsoncpp/include/json/*.h",
		"src/external/jsoncpp/src/lib_json/*.h",
		"src/external/jsoncpp/src/lib_json/*.inl",
		"src/external/jsoncpp/src/lib_json/*.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"NOMINMAX"
	}

	disablewarnings
	{
		"4244",
		"4267",
		"4996",
		"6386"
	}

	filter "system:windows"
		defines
		{
			"XR_OS_WINDOWS",
			"XR_USE_PLATFORM_WIN32",
			"XR_USE_GRAPHICS_API_D3D11"
		}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		optimize "On"

	filter "configurations:Final"
		runtime "Release"
		optimize "On"

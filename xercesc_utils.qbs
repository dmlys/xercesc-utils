import qbs
import qbs.Environment

StaticLibrary
{
	Depends { name: "cpp" }
	Depends { name: "extlib" }
	Depends { name: "ProjectSettings"; required: false }

	cpp.cxxLanguageVersion : "c++17"
	cpp.cxxFlags: project.additionalCxxFlags
	cpp.driverFlags: project.additionalDriverFlags
	cpp.defines: project.additionalDefines
	cpp.systemIncludePaths: project.additionalSystemIncludePaths
	cpp.includePaths: ["include"].concat(project.additionalIncludePaths || [])
	cpp.libraryPaths: project.additionalLibraryPaths


	Export
	{
		Depends { name: "cpp" }

		cpp.cxxLanguageVersion : "c++17"
		cpp.includePaths : ["include"]
	}

	files: [
		"include/xercesc/**",
		"src/**",
	]
}

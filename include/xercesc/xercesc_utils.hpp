#pragma once
#include <memory>
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <istream>
#include <ostream>
#include <xercesc/xercesc_include.h>

namespace xercesc_utils
{

#if WCHAR_MAX == ISHORT_MAX
#define XERCESC_LIT(str) L##str
#else
#define XERCESC_LIT(str) u##str
#endif

	using xml_string      = std::basic_string<XMLCh>;
	using xml_string_view = std::basic_string_view<XMLCh>;

	extern const std::string empty_string;

	class xml_path_exception : public std::runtime_error
	{
	public:
		xml_path_exception(const std::string & path);
		xml_path_exception(xml_string_view path);
	};

	class xercesc_release_deleter
	{
	public:
		template <class Type>
		void operator()(Type * ptr) const noexcept
		{
			ptr->release();
		}
	};

	class xercesc_string_deleter
	{
	public:
		void operator()(XMLCh * str) const noexcept
		{
			xercesc::XMLString::release(&str);
		}
	};
	
	// some xercesc smart pointers
	using XMLChPtr = std::unique_ptr<XMLCh, xercesc_string_deleter> ;

	using XercesDOMParserPtr = std::unique_ptr<xercesc::XercesDOMParser>;
	using HandlerBasePtr     = std::unique_ptr<xercesc::HandlerBase>;

	using DOMXPathNSResolverPtr = std::unique_ptr<xercesc::DOMXPathNSResolver, xercesc_release_deleter>;

	using DOMDocumentPtr     = std::unique_ptr<xercesc::DOMDocument, xercesc_release_deleter>;
	using DOMLSSerializerPtr = std::unique_ptr<xercesc::DOMLSSerializer, xercesc_release_deleter>;
	using DOMLSOutputPtr     = std::unique_ptr<xercesc::DOMLSOutput, xercesc_release_deleter>;


	std::string to_ansi(const XMLCh * str, std::size_t len = -1);
	std::string to_utf8(const XMLCh * utf16_str, std::size_t len = -1);
	xml_string  to_xmlch(const char * utf8_str, std::size_t len = -1);

	inline std::string to_utf8(xml_string_view  utf16_str) { return to_utf8(utf16_str.data(), utf16_str.size()); }
	inline std::string to_ansi(xml_string_view  utf16_str) { return to_ansi(utf16_str.data(), utf16_str.size()); }
	inline xml_string  to_xmlch(std::string_view utf8_str) { return to_xmlch(utf8_str.data(), utf8_str.size());  }

	template <class String> std::enable_if_t<std::is_convertible_v<String, xml_string_view>,  xml_string> forward_as_xml_string(const String & str) { return xml_string(str); }
	template <class String> std::enable_if_t<std::is_convertible_v<String, std::string_view>, xml_string> forward_as_xml_string(const String & str) { return to_xmlch(std::string_view(str)); }

	template <class String> std::enable_if_t<std::is_convertible_v<String, xml_string_view>, xml_string_view> forward_xml_string_view(const String & str) { return xml_string_view(str); }
	template <class String> std::enable_if_t<std::is_convertible_v<String, std::string_view>,     xml_string> forward_xml_string_view(const String & str) { return to_xmlch(std::string_view(str)); }

	/// формирует сообщение об ошибке в формате:
	/// $message, at line $line, column $column
	std::string error_report(xercesc::SAXParseException & ex);
	std::string error_report(xercesc::XMLException & ex);


	DOMDocumentPtr create_empty_document();

	enum save_option : bool
	{
		pretty_print = true, as_is = false,
	};
	
	std::string print(xercesc::DOMElement * element, save_option save_option = pretty_print);
	std::string save(xercesc::DOMDocument * doc, save_option save_option = pretty_print, const XMLCh * encoding = XERCESC_LIT("utf-8"));
	void save_to_file(xercesc::DOMDocument * doc, const xml_string  & file, save_option save_option = pretty_print, const XMLCh * encoding = XERCESC_LIT("utf-8"));
	void save_to_file(xercesc::DOMDocument * doc, const std::string & file, save_option save_option = pretty_print, const XMLCh * encoding = XERCESC_LIT("utf-8"));

	void save(std::streambuf & sb, xercesc::DOMDocument * doc, save_option save_option = pretty_print, const XMLCh * encoding = XERCESC_LIT("utf-8"));
	void save(std::ostream & os, xercesc::DOMDocument * doc, save_option save_option = pretty_print, const XMLCh * encoding = XERCESC_LIT("utf-8"));

	std::shared_ptr<xercesc::DOMDocument> load(const std::string & content);
	std::shared_ptr<xercesc::DOMDocument> load(const char * data, std::size_t size);
	std::shared_ptr<xercesc::DOMDocument> load_from_file(const xml_string  & file);
	std::shared_ptr<xercesc::DOMDocument> load_from_file(const std::string & file);

	std::shared_ptr<xercesc::DOMDocument> load(std::streambuf & is);
	std::shared_ptr<xercesc::DOMDocument> load(std::istream & is);

	class DOMXPathNSResolverImpl : public xercesc::DOMXPathNSResolver
	{
		std::unordered_map<xml_string, xml_string> m_uri_mappings;
		std::unordered_map<xml_string, xml_string> m_prefix_mappings;

	public:
		void addNamespaceBinding(std::string_view prefix, std::string_view uri) { return addNamespaceBinding(to_xmlch(prefix), to_xmlch(uri)); }
		void addNamespaceBinding(xml_string_view  prefix, xml_string_view  uri) { return addNamespaceBinding(xml_string(prefix.data(), prefix.size()), xml_string(uri.data(), uri.size())); }
		void addNamespaceBinding(xml_string prefix, xml_string uri);

		void addNamespaceBinding(const XMLCh * prefix, const XMLCh * uri) override;
		void release() override;

		const XMLCh * lookupNamespaceURI(const XMLCh* prefix) const override;
		const XMLCh * lookupPrefix(const XMLCh* URI) const override;

	public:
		DOMXPathNSResolverImpl() = default;
		virtual ~DOMXPathNSResolverImpl() = default;
	};



	std::string get_text_content(xercesc::DOMElement * element);

	xercesc::DOMElement * find_child(xercesc::DOMElement * element, xml_string_view name);
	xercesc::DOMElement * find_path(xercesc::DOMElement * element,  xml_string_view path);
	xercesc::DOMElement * find_path(xercesc::DOMDocument * doc,     xml_string_view path);

	xercesc::DOMElement * get_child(xercesc::DOMElement * element, xml_string_view name);
	xercesc::DOMElement * get_path(xercesc::DOMElement * element,  xml_string_view path);
	xercesc::DOMElement * get_path(xercesc::DOMDocument * doc,     xml_string_view path);

	xercesc::DOMElement * acquire_path(xercesc::DOMElement * element, xml_string path);
	xercesc::DOMElement * acquire_path(xercesc::DOMDocument * doc,    xml_string path);

	std::string find_path_text(xercesc::DOMDocument * doc, xml_string_view path, std::string_view defval = empty_string);
	std::string find_path_text(xercesc::DOMElement * element, xml_string_view path, std::string_view defval = empty_string);

	std::string get_path_text(xercesc::DOMDocument * doc, xml_string_view path);
	std::string get_path_text(xercesc::DOMElement * element, xml_string_view path);

	void set_path_text(xercesc::DOMDocument * doc, xml_string path, std::string_view value);
	void set_path_text(xercesc::DOMElement * elem, xml_string path, std::string_view value);


	template <class String>	inline xercesc::DOMElement * find_child(xercesc::DOMElement * element, const String & path)	{ return find_child(element, forward_xml_string_view(path)); }
	template <class String>	inline xercesc::DOMElement * find_path (xercesc::DOMElement * element, const String & path) { return find_path (element, forward_xml_string_view(path)); }
	template <class String>	inline xercesc::DOMElement * find_path (xercesc::DOMDocument * doc,    const String & path) { return find_path (doc,     forward_xml_string_view(path)); }

	template <class String> inline xercesc::DOMElement * get_child(xercesc::DOMElement * element, const String & path)  { return get_child(element, forward_xml_string_view(path)); }
	template <class String> inline xercesc::DOMElement * get_path (xercesc::DOMElement * element, const String & path)  { return get_path (element, forward_xml_string_view(path)); }
	template <class String> inline xercesc::DOMElement * get_path (xercesc::DOMDocument * doc,    const String & path)  { return get_path (doc,     forward_xml_string_view(path)); }


	template <class String> inline xercesc::DOMElement * acquire_path(xercesc::DOMElement * element, const String & path) {	return acquire_path(element, forward_as_xml_string(path)); }
	template <class String>	inline xercesc::DOMElement * acquire_path(xercesc::DOMDocument * doc,    const String & path) { return acquire_path(doc,     forward_as_xml_string(path)); }

	template <class String>	inline std::string find_path_text(xercesc::DOMElement * element, const String & path, std::string_view defval = empty_string) { return find_path_text(element, forward_xml_string_view(path), defval); }
	template <class String> inline std::string find_path_text(xercesc::DOMDocument * doc,    const String & path, std::string_view defval = empty_string) { return find_path_text(doc,     forward_xml_string_view(path), defval); }

	template <class String>	inline std::string get_path_text(xercesc::DOMElement * element, const String & path) { return get_path_text(element, forward_xml_string_view(path)); }
	template <class String>	inline std::string get_path_text(xercesc::DOMDocument * doc,    const String & path) { return get_path_text(doc,     forward_xml_string_view(path)); }

	template <class String> inline void set_path_text(xercesc::DOMElement * element, const String & path, std::string_view value) { return set_path_text(element, forward_xml_string_view(path), value); }
	template <class String>	inline void set_path_text(xercesc::DOMDocument * doc,    const String & path, std::string_view value) { return set_path_text(doc,     forward_xml_string_view(path), value); }

	xercesc::DOMElement * rename_subtree(xercesc::DOMElement * element, const xml_string & namespace_uri, const xml_string & prefix);
	xercesc::DOMNode *    rename_subtree(xercesc::DOMNode *    node,    const xml_string & namespace_uri, const xml_string & prefix);

	template <class UriString, class PrefixString>
	xercesc::DOMElement * rename_subtree(xercesc::DOMElement * element, const UriString & namespace_uri, const PrefixString & prefix)
	{ return rename_subtree(element, forward_as_xml_string(namespace_uri), forward_as_xml_string(prefix)); }

	template <class UriString, class PrefixString>
	xercesc::DOMElement * rename_subtree(xercesc::DOMNode * node, const UriString & namespace_uri, const PrefixString & prefix)
	{ return rename_subtree(node, forward_as_xml_string(namespace_uri), forward_as_xml_string(prefix)); }
}

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
	#define XERCESC_LIT_IMPL_wchar_t(str) L##str
	#define XERCESC_LIT_IMPL_char8_t(str) u8##str
	#define XERCESC_LIT_IMPL_char16_t(str) u##str
	#define XERCESC_LIT_IMPL_char32_t(str) U##str
	
	#define XERCESC_LIT_IMPL2(str, char_type) XERCESC_LIT_IMPL_##char_type(str)
	#define XERCESC_LIT_IMPL(str, char_type) XERCESC_LIT_IMPL2(str, char_type)
	#define XERCESC_LIT(str) XERCESC_LIT_IMPL(str, XERCES_XMLCH_T)
	
	
	inline void xercesc_init() { xercesc::XMLPlatformUtils::Initialize(); }
	inline void xercesc_free() { xercesc::XMLPlatformUtils::Terminate();  }


	using xml_string      = std::basic_string<XMLCh>;
	using xml_string_view = std::basic_string_view<XMLCh>;

	extern const std::string empty_string;

	class xml_path_exception : public std::runtime_error
	{
	public:
		xml_path_exception(const std::string & path);
		xml_path_exception(xml_string_view path);
	};

	class xml_namespace_exception : public std::runtime_error
	{
	public:
		xml_namespace_exception(const std::string & msg);
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
	using DOMXPathResultPtr  = std::unique_ptr<xercesc::DOMXPathResult, xercesc_release_deleter>;


	std::string to_ansi(const XMLCh * str, std::size_t len = -1);
	std::string to_utf8(const XMLCh * utf16_str, std::size_t len = -1);
	xml_string  to_xmlch(const char * utf8_str, std::size_t len = -1);

	inline std::string to_utf8(xml_string_view  utf16_str) { return to_utf8(utf16_str.data(), utf16_str.size()); }
	inline std::string to_ansi(xml_string_view  utf16_str) { return to_ansi(utf16_str.data(), utf16_str.size()); }
	inline xml_string  to_xmlch(std::string_view utf8_str) { return to_xmlch(utf8_str.data(), utf8_str.size());  }

	inline xml_string && forward_as_xml_string(xml_string && str) { return std::move(str); }
	inline xml_string    forward_as_xml_string(const XMLCh * str) { return str ? xml_string(str) : xml_string(); }
	inline xml_string    forward_as_xml_string(const char  * str) { return to_xmlch(str, -1); /* to_xmlch checks for nullptr */ }
	
	template <class String> std::enable_if_t<std::is_convertible_v<String, xml_string_view>,  xml_string> forward_as_xml_string(const String & str) { return xml_string(str); }
	template <class String> std::enable_if_t<std::is_convertible_v<String, std::string_view>, xml_string> forward_as_xml_string(const String & str) { return to_xmlch(std::string_view(str)); }

	inline xml_string_view forward_xml_string_view(const XMLCh * str) { return str ? xml_string_view(str) : xml_string_view(); }
	inline xml_string      forward_xml_string_view(const char  * str) { return to_xmlch(str, -1); /* to_xmlch checks for nullptr */ }
	
	template <class String> std::enable_if_t<std::is_convertible_v<String, xml_string_view>, xml_string_view> forward_xml_string_view(const String & str) { return xml_string_view(str); }
	template <class String> std::enable_if_t<std::is_convertible_v<String, std::string_view>,     xml_string> forward_xml_string_view(const String & str) { return to_xmlch(std::string_view(str)); }


	/// формирует сообщение об ошибке в формате:
	/// $message, at line $line, column $column
	std::string error_report(xercesc::SAXParseException & ex);
	std::string error_report(xercesc::XMLException & ex);


	std::shared_ptr<xercesc::DOMDocument> create_empty_document();

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

	std::shared_ptr<xercesc::DOMDocument> load(std::string_view str);
	std::shared_ptr<xercesc::DOMDocument> load(const char * data, std::size_t size);
	std::shared_ptr<xercesc::DOMDocument> load_from_file(const xml_string  & file);
	std::shared_ptr<xercesc::DOMDocument> load_from_file(const std::string & file);

	std::shared_ptr<xercesc::DOMDocument> load(std::streambuf & is);
	std::shared_ptr<xercesc::DOMDocument> load(std::istream & is);

	class DOMXPathNSResolverImpl;
	using DOMXPathNSResolverImplPtr = std::unique_ptr<DOMXPathNSResolverImpl, xercesc_release_deleter>;

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
		DOMXPathNSResolverImpl * clone() const;

	public:
		DOMXPathNSResolverImpl() = default;
		virtual ~DOMXPathNSResolverImpl() = default;
	};


	DOMXPathNSResolverImplPtr create_resolver(std::initializer_list<std::pair<std::string_view, std::string_view>> items);
	DOMXPathNSResolverImplPtr create_resolver(std::initializer_list<std::pair<xml_string, xml_string>> items);
\
	/************************************************************************/
	/*                      namespace helpers                               */
	/************************************************************************/
	void associate_resolver(xercesc::DOMNode * node, xercesc::DOMXPathNSResolver * resolver);
	auto get_associated_resolver(xercesc::DOMNode * node) -> xercesc::DOMXPathNSResolver *;
	inline void associate_resolver(xercesc::DOMNode * node, DOMXPathNSResolverPtr resolver) { return associate_resolver(node, resolver.release()); }

	auto associate_namespaces(xercesc::DOMNode * node, std::initializer_list<std::pair<std::string_view, std::string_view>> items) -> DOMXPathNSResolverImpl *;
	auto associate_namespaces(xercesc::DOMNode * node, std::initializer_list<std::pair<xml_string, xml_string>> items) -> DOMXPathNSResolverImpl *;

	void set_namespace(xercesc::DOMElement  * element, xml_string prefix, xml_string uri);
	void set_namespace(xercesc::DOMDocument * doc,     xml_string prefix, xml_string uri);

	template <class PrefixString, class UriString> inline void set_namespace(xercesc::DOMElement  * element, const PrefixString & prefix, const UriString & uri) { return set_namespace(element, forward_as_xml_string(prefix), forward_as_xml_string(uri)); }
	template <class PrefixString, class UriString> inline void set_namespace(xercesc::DOMDocument * doc,     const PrefixString & prefix, const UriString & uri) { return set_namespace(doc,     forward_as_xml_string(prefix), forward_as_xml_string(uri)); }

	void set_namespaces(xercesc::DOMDocument * doc, std::initializer_list<std::pair<std::string_view, std::string_view>> items);
	void set_namespaces(xercesc::DOMDocument * doc, std::initializer_list<std::pair<xml_string, xml_string>> items);

	/************************************************************************/
	/*                    create helpers                                    */
	/************************************************************************/
	xercesc::DOMElement * create_element_ns(xercesc::DOMElement * element, xml_string namespace_uri, xml_string qualified_name);
	xercesc::DOMAttr  * create_attribute_ns(xercesc::DOMElement * element, xml_string namespace_uri, xml_string qualified_name);
	
	void set_attribute_ns(xercesc::DOMElement * element, xml_string namespace_uri, xml_string qualified_name, std::string_view value);
	
	template <class NamespaceString, class NameString> inline xercesc::DOMElement * create_element_ns(xercesc::DOMElement * element, const NamespaceString & namespace_uri, const NameString & qualified_name) { return create_element_ns(element, forward_as_xml_string(namespace_uri), forward_as_xml_string(qualified_name)); }
	template <class NamespaceString, class NameString> inline xercesc::DOMAttr  * create_attribute_ns(xercesc::DOMElement * element, const NamespaceString & namespace_uri, const NameString & qualified_name) { return create_attribute_ns(element, forward_as_xml_string(namespace_uri), forward_as_xml_string(qualified_name)); }
	template <class NamespaceString, class NameString> inline void set_attribute_ns(xercesc::DOMElement * element, const NamespaceString & namespace_uri, const NameString & qualified_name, std::string_view value) { return set_attribute_ns(element, forward_as_xml_string(namespace_uri), forward_as_xml_string(qualified_name), std::move(value)); }
	
	/************************************************************************/
	/*                    basic path helpers                                */
	/************************************************************************/
	std::string get_text_content(xercesc::DOMElement * element);
	       void set_text_content(xercesc::DOMElement * element, std::string_view text);

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

	/************************************************************************/
	/*                        attribute helpers                             */
	/************************************************************************/
	xercesc::DOMAttr * find_attribute_node(xercesc::DOMElement * element, const xml_string & attrname);
	xercesc::DOMAttr *  get_attribute_node(xercesc::DOMElement * element, const xml_string & attrname);

	std::string find_attribute_text(xercesc::DOMElement * element, const xml_string & attrname, std::string_view defval = empty_string);
	std::string  get_attribute_text(xercesc::DOMElement * element, const xml_string & attrname);
	       void  set_attribute_text(xercesc::DOMElement * element, const xml_string & attrname, std::string_view text);

	template <class String> inline xercesc::DOMAttr * find_attribute_node(xercesc::DOMElement * element, const xml_string & attrname) { return find_attribute_node(element, forward_as_xml_string(attrname)); }
	template <class String> inline xercesc::DOMAttr *  get_attribute_node(xercesc::DOMElement * element, const xml_string & attrname) { return  get_attribute_node(element, forward_as_xml_string(attrname)); }

	template <class String> inline std::string find_attribute_text(xercesc::DOMElement * element, const String & attrname, std::string_view defval = empty_string) { return find_attribute_text(element, forward_as_xml_string(attrname), defval); }
	template <class String> inline std::string  get_attribute_text(xercesc::DOMElement * element, const String & attrname)                                         { return  get_attribute_text(element, forward_as_xml_string(attrname));         }
	template <class String> inline        void  set_attribute_text(xercesc::DOMElement * element, const String & attrname, std::string_view text)                  { return  set_attribute_text(element, forward_as_xml_string(attrname), text);   }

	/************************************************************************/
	/*                        rename subtree group                          */
	/************************************************************************/
	xercesc::DOMElement * rename_subtree(xercesc::DOMElement * element, const xml_string & namespace_uri, const xml_string & prefix);
	xercesc::DOMNode *    rename_subtree(xercesc::DOMNode *    node,    const xml_string & namespace_uri, const xml_string & prefix);

	template <class UriString, class PrefixString>
	xercesc::DOMElement * rename_subtree(xercesc::DOMElement * element, const UriString & namespace_uri, const PrefixString & prefix)
	{ return rename_subtree(element, forward_as_xml_string(namespace_uri), forward_as_xml_string(prefix)); }

	template <class UriString, class PrefixString>
	xercesc::DOMElement * rename_subtree(xercesc::DOMNode * node, const UriString & namespace_uri, const PrefixString & prefix)
	{ return rename_subtree(node, forward_as_xml_string(namespace_uri), forward_as_xml_string(prefix)); }


	/************************************************************************/
	/*                        xpath helpers                                 */
	/************************************************************************/
	xercesc::DOMElement * find_xpath(xercesc::DOMElement * element, const xml_string & path);
	xercesc::DOMElement * find_xpath(xercesc::DOMElement * element, const xml_string & path, xercesc::DOMXPathNSResolver * resolver);

	xercesc::DOMElement * get_xpath(xercesc::DOMElement * element, const xml_string & path);
	xercesc::DOMElement * get_xpath(xercesc::DOMElement * element, const xml_string & path, xercesc::DOMXPathNSResolver * resolver);

	std::string find_xpath_text(xercesc::DOMElement * element, const xml_string & path, std::string_view defval = empty_string);
	std::string  get_xpath_text(xercesc::DOMElement * element, const xml_string & path);

	template <class String> inline xercesc::DOMElement * find_xpath(xercesc::DOMElement * element, const String & path)                                         { return find_xpath(element, forward_as_xml_string(path)); }
	template <class String> inline xercesc::DOMElement * find_xpath(xercesc::DOMElement * element, const String & path, xercesc::DOMXPathNSResolver * resolver) { return find_xpath(element, forward_as_xml_string(path), resolver); }

	template <class String> inline xercesc::DOMElement * get_xpath(xercesc::DOMElement * element, const String & path)                                          { return find_xpath(element, forward_as_xml_string(path)); }
	template <class String> inline xercesc::DOMElement * get_xpath(xercesc::DOMElement * element, const String & path, xercesc::DOMXPathNSResolver * resolver)  { return  get_xpath(element, forward_as_xml_string(path), resolver); }

	template <class String> inline std::string find_xpath_text(xercesc::DOMElement * element, const String & path, std::string_view defval = empty_string)      { return find_xpath_text(element, forward_as_xml_string(path), defval); }
	template <class String> inline std::string  get_xpath_text(xercesc::DOMElement * element, const String & path)                                              { return  get_xpath_text(element, forward_as_xml_string(path)); }
	
	// document overloads
	template <class String> inline xercesc::DOMElement * find_xpath(xercesc::DOMDocument * doc, const String & path)                                         { return find_xpath(doc->getDocumentElement(), forward_as_xml_string(path)); }
	template <class String> inline xercesc::DOMElement * find_xpath(xercesc::DOMDocument * doc, const String & path, xercesc::DOMXPathNSResolver * resolver) { return find_xpath(doc->getDocumentElement(), forward_as_xml_string(path), resolver); }

	template <class String> inline xercesc::DOMElement * get_xpath(xercesc::DOMDocument * doc, const String & path)                                          { return find_xpath(doc->getDocumentElement(), forward_as_xml_string(path)); }
	template <class String> inline xercesc::DOMElement * get_xpath(xercesc::DOMDocument * doc, const String & path, xercesc::DOMXPathNSResolver * resolver)  { return  get_xpath(doc->getDocumentElement(), forward_as_xml_string(path), resolver); }

	template <class String> inline std::string find_xpath_text(xercesc::DOMDocument * doc, const String & path, std::string_view defval = empty_string)      { return find_xpath_text(doc->getDocumentElement(), forward_as_xml_string(path), defval); }
	template <class String> inline std::string  get_xpath_text(xercesc::DOMDocument * doc, const String & path)                                              { return  get_xpath_text(doc->getDocumentElement(), forward_as_xml_string(path)); }
}

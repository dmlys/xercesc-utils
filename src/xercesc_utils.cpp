#include <codecvt>
#include <ext/codecvt_conv.hpp>
#include <xercesc/xercesc_utils.hpp>
#include <boost/predef.h>

namespace xercesc_utils
{
	const std::codecvt_utf8_utf16<XMLCh, 0x10FFFF, std::codecvt_mode::little_endian> u8_cvt;
	const std::string empty_string;
	const XMLCh separator = '/';

	xml_path_exception::xml_path_exception(const std::string & path)
	    : std::runtime_error("\"" + path + "\" not found") {}

	xml_path_exception::xml_path_exception(xml_string_view path)
	    : std::runtime_error("\"" + to_utf8(path) + "\" not found") {}

	xml_namespace_exception::xml_namespace_exception(const std::string & msg)
	    : std::runtime_error(msg) {}

	std::string to_utf8(const XMLCh * str, std::size_t len)
	{
		if (str == nullptr) return "";
		if (len == std::size_t(-1)) len = std::char_traits<XMLCh>::length(str);

		auto input = boost::make_iterator_range_n(str, len);
		return ext::codecvt_convert::to_bytes(u8_cvt, input);
	}

	std::string to_ansi(const XMLCh * str, std::size_t len)
	{
		if (str == nullptr) return "";
		if (len == std::size_t(-1)) len = std::char_traits<XMLCh>::length(str);

#if BOOST_LIB_STD_DINKUMWARE
		// MSVC stdlib does have problems with std::codecvt<char16_t, char, std::mbstate_t>;
		std::locale loc;
		using cvt_type = std::codecvt<wchar_t, char, std::mbstate_t>;
		auto & cvt = std::use_facet<cvt_type>(loc);
		
		static_assert(sizeof(wchar_t) == sizeof(XMLCh));
		auto input = boost::make_iterator_range_n(reinterpret_cast<const wchar_t *>(str), len);
		return ext::codecvt_convert::to_bytes(cvt, input);
#else
		std::locale loc;
		using cvt_type = std::codecvt<XMLCh, char, std::mbstate_t>;
		auto & cvt = std::use_facet<cvt_type>(loc);
		
		auto input = boost::make_iterator_range_n(str, len);
		return ext::codecvt_convert::to_bytes(cvt, input);
#endif

	}

	xercesc_utils::xml_string to_xmlch(const char * utf8_str, std::size_t len)
	{
		xercesc_utils::xml_string res;
		if (utf8_str == nullptr) return res;
		if (len == std::size_t(-1)) len = std::char_traits<char>::length(utf8_str);
		
		auto input = boost::make_iterator_range_n(utf8_str, len);
		ext::codecvt_convert::from_bytes(u8_cvt, input, res);

		return res;
	}

	namespace
	{
		/************************************************************************/
		/*                   iostream helper classes                            */
		/************************************************************************/
		class streambuf_bin_source : public xercesc::BinInputStream
		{
			std::streambuf * m_sb;
			std::size_t m_curpos = 0;
			// BinInputStream interface
		public:
			XMLFilePos curPos() const { return m_curpos; }
			XMLSize_t readBytes(XMLByte * const toFill, const XMLSize_t maxToRead);
			const XMLCh * getContentType() const { return nullptr; }

		public:
			streambuf_bin_source(std::streambuf * sb) : m_sb(sb) {}
			~streambuf_bin_source() = default;
		};

		XMLSize_t streambuf_bin_source::readBytes(XMLByte * toFill, XMLSize_t maxToRead)
		{
			XMLSize_t read = m_sb->sgetn(reinterpret_cast<char *>(toFill), maxToRead);
			//if (read < maxToRead)
			//	throw std::runtime_error("xercesc_utils::streambuf_bin_source: failed to read from std::streambuf");

			m_curpos += read;
			return read;
		}

		class streambuf_input_source : public xercesc::InputSource
		{
			std::streambuf * m_sb;

		public:
			xercesc_3_2::BinInputStream * makeStream() const { return new streambuf_bin_source(m_sb); }

		public:
			streambuf_input_source(std::streambuf * sb) : m_sb(sb) {}
		};

		class streambuf_target : public xercesc::XMLFormatTarget
		{
			std::streambuf * m_sb;
		public:
			void writeChars(const XMLByte * const toWrite, const XMLSize_t count, xercesc_3_2::XMLFormatter * const formatter);
			void flush();

		public:
			streambuf_target(std::streambuf * buf) : m_sb(buf) {}
		};

		void streambuf_target::writeChars(const XMLByte * toWrite, const XMLSize_t count, xercesc_3_2::XMLFormatter * formatter)
		{
			XMLSize_t written = m_sb->sputn(reinterpret_cast<const char *>(toWrite), count);
			if (written < count)
				throw std::runtime_error("xercesc_utils::streambuf_target: failed to write to std::streambuf");

			return;
		}

		void streambuf_target::flush()
		{
			auto res = m_sb->pubsync();
			if (res != 0)
				throw std::runtime_error("xercesc_utils::streambuf_target: failed to sync std::streambuf");
		}
	} // 'anonymous' namespace

	std::string error_report(xercesc::SAXParseException & ex)
	{
		std::string report;
		report.reserve(1024);

		report += to_utf8(ex.getMessage());
		report += ", at line ";
		report += std::to_string(ex.getLineNumber());
		report += ", column ";
		report += std::to_string(ex.getColumnNumber());

		return report;
	}

	std::string error_report(xercesc::XMLException & ex)
	{
		std::string report;
		report.reserve(1024);

		report += ex.getSrcFile();
		report += "(";
		report += std::to_string(ex.getSrcLine());
		report += ")";
		report += ": ";
		report += to_utf8(ex.getMessage());

		return report;
	}

	std::shared_ptr<xercesc::DOMDocument> create_empty_document()
	{
		const XMLCh * tempStr = XERCESC_LIT("LS");
		xercesc::DOMImplementation  * impl = xercesc::DOMImplementationRegistry::getDOMImplementation(tempStr);
		std::shared_ptr<xercesc::DOMDocument> domptr(impl->createDocument(), xercesc_release_deleter());

		auto * conf = domptr->getDOMConfig();
		// Ignore comments and whitespace so we don't get extra nodes to process that just waste time.
		// conf->setParameter(xercesc::XMLUni::fgDOMComments, false);
		// conf->setParameter(xercesc::XMLUni::fgDOMElementContentWhitespace, false);

		// Setup some properties that look like they might be required to get namespaces to work but doesn't seem to help at all.
		// conf->setParameter(xercesc::XMLUni::fgXercesUseCachedGrammarInParse, true);
		conf->setParameter(xercesc::XMLUni::fgDOMNamespaces, true);
		conf->setParameter(xercesc::XMLUni::fgDOMNamespaceDeclarations, true);

		return domptr;
	}

	std::string print(xercesc::DOMElement * element, save_option save_option /* = pretty_print */)
	{
		using namespace xercesc;
		if (not element) throw std::invalid_argument("xercesc_utils::print: element is null");
		//if (not encoding) throw std::invalid_argument("xercesc_utils::save: encoding is null");

		// get a serializer, an instance of DOMLSSerializer
		const XMLCh * tempStr = XERCESC_LIT("LS");
		DOMImplementation  * impl = xercesc::DOMImplementationRegistry::getDOMImplementation(tempStr);
		DOMLSSerializerPtr   theSerializer ( ((xercesc::DOMImplementationLS*)impl)->createLSSerializer() );
		DOMConfiguration   * serializerConfig = theSerializer->getDomConfig();

		if (save_option == pretty_print)
		{
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTXercesPrettyPrint, false); // fixes double new-line
		}

		XMLChPtr xml ( theSerializer->writeToString(element) );
		return to_utf8(xml.get());
	}

	std::string save(xercesc::DOMDocument * document, save_option save_option /* = pretty_print */, const XMLCh * encoding /* = L"utf-8" */)
	{
		using namespace xercesc;
		if (not document) throw std::invalid_argument("xercesc_utils::save: document is null");
		if (not encoding) throw std::invalid_argument("xercesc_utils::save: encoding is null");

		// get a serializer, an instance of DOMLSSerializer
		const XMLCh * tempStr = XERCESC_LIT("LS");
		DOMImplementation  * impl = xercesc::DOMImplementationRegistry::getDOMImplementation(tempStr);
		DOMLSSerializerPtr   theSerializer ( ((xercesc::DOMImplementationLS*)impl)->createLSSerializer() );
		DOMLSOutputPtr       theOutputDesc ( ((xercesc::DOMImplementationLS*)impl)->createLSOutput() );
		DOMConfiguration   * serializerConfig = theSerializer->getDomConfig();

		serializerConfig->setParameter(xercesc::XMLUni::fgDOMXMLDeclaration, true);
		if (save_option)
		{
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTXercesPrettyPrint, false); // fixes double new-line
		}

		theOutputDesc->setEncoding(encoding);

		MemBufFormatTarget memTarget;
		theOutputDesc->setByteStream(&memTarget);
		theSerializer->write(document, theOutputDesc.get());

		auto * buf = memTarget.getRawBuffer();
		auto len = memTarget.getLen();

		return std::string(reinterpret_cast<const char *>(buf), len);
	}

	void save_to_file(xercesc::DOMDocument * document, const xml_string & file, save_option save_option /* = pretty_print */, const XMLCh * encoding /* = L"utf-8" */)
	{
		using namespace xercesc;
		if (not document) throw std::invalid_argument("xercesc_utils::save: document is null");
		if (not encoding) throw std::invalid_argument("xercesc_utils::save: encoding is null");

		// get a serializer, an instance of DOMLSSerializer
		const XMLCh * tempStr = XERCESC_LIT("LS");
		DOMImplementation  * impl = xercesc::DOMImplementationRegistry::getDOMImplementation(tempStr);
		DOMLSSerializerPtr   theSerializer(((xercesc::DOMImplementationLS*)impl)->createLSSerializer());
		DOMLSOutputPtr       theOutputDesc(((xercesc::DOMImplementationLS*)impl)->createLSOutput());
		DOMConfiguration   * serializerConfig = theSerializer->getDomConfig();

		serializerConfig->setParameter(xercesc::XMLUni::fgDOMXMLDeclaration, true);
		if (save_option == pretty_print)
		{
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTXercesPrettyPrint, false); // fixes double new-line
		}

		theOutputDesc->setEncoding(encoding);

		LocalFileFormatTarget fileTarget(file.c_str());
		theOutputDesc->setByteStream(&fileTarget);
		theSerializer->write(document, theOutputDesc.get());
	}

	void save_to_file(xercesc::DOMDocument * document, const std::string & file, save_option save_option /* = pretty_print */, const XMLCh * encoding /* = L"utf-8" */)
	{
		return save_to_file(document, to_xmlch(file), save_option, encoding);
	}

	void save(std::streambuf & sb, xercesc::DOMDocument * document, save_option save_option /* = pretty_print */, const XMLCh * encoding /* = XERCESC_LIT("utf-8") */)
	{
		using namespace xercesc;
		if (not document) throw std::invalid_argument("xercesc_utils::save: document is null");
		if (not encoding) throw std::invalid_argument("xercesc_utils::save: encoding is null");

		// get a serializer, an instance of DOMLSSerializer
		const XMLCh * tempStr = XERCESC_LIT("LS");
		DOMImplementation  * impl = xercesc::DOMImplementationRegistry::getDOMImplementation(tempStr);
		DOMLSSerializerPtr   theSerializer(((xercesc::DOMImplementationLS*)impl)->createLSSerializer());
		DOMLSOutputPtr       theOutputDesc(((xercesc::DOMImplementationLS*)impl)->createLSOutput());
		DOMConfiguration   * serializerConfig = theSerializer->getDomConfig();

		serializerConfig->setParameter(xercesc::XMLUni::fgDOMXMLDeclaration, true);
		if (save_option == pretty_print)
		{
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);
			serializerConfig->setParameter(xercesc::XMLUni::fgDOMWRTXercesPrettyPrint, false); // fixes double new-line
		}

		theOutputDesc->setEncoding(encoding);

		streambuf_target target(&sb);
		theOutputDesc->setByteStream(&target);
		theSerializer->write(document, theOutputDesc.get());
	}

	void save(std::ostream & os, xercesc::DOMDocument * doc, save_option save_option /* = pretty_print */, const XMLCh * encoding /* = XERCESC_LIT("utf-8") */)
	{
		auto * sbuf = os.rdbuf();
		if (not sbuf) throw std::invalid_argument("xercesc_utils::save: std::ostream does not have streambuf!");
		return save(*sbuf, doc, save_option, encoding);
	}

	template <class Functor>
	static auto wrapped_load_xml(Functor && functor)
	{
		try
		{
			return std::forward<Functor>(functor)();
		}
		catch (xercesc::XMLException & ex)
		{
			std::throw_with_nested(std::runtime_error(error_report(ex)));
		}
		catch (xercesc::DOMException & ex)
		{
			std::throw_with_nested(std::runtime_error(xercesc_utils::to_utf8(ex.getMessage())));
		}
		catch (xercesc::SAXParseException & ex)
		{
			std::throw_with_nested(std::runtime_error(error_report(ex)));
		}
		catch (xercesc::SAXException & ex)
		{
			std::throw_with_nested(std::runtime_error(xercesc_utils::to_utf8(ex.getMessage())));
		}
	}

	std::shared_ptr<xercesc::DOMDocument> load(const std::string & str)
	{
		return load(str.data(), str.size());
	}

	std::shared_ptr<xercesc::DOMDocument> load(const char * data, std::size_t size)
	{
		return wrapped_load_xml([data, size]
		{
			auto parser = std::make_shared<xercesc::XercesDOMParser>();
			xercesc_utils::HandlerBasePtr err(new xercesc::HandlerBase);

			xercesc::MemBufInputSource input(reinterpret_cast<const XMLByte *>(data), size, "input buffer");
			input.setCopyBufToStream(false);

			parser->setDoNamespaces(true);
			parser->setErrorHandler(err.get());
			parser->parse(input);
			parser->setErrorHandler(nullptr);

			// aliasing constructor
			return std::shared_ptr<xercesc::DOMDocument>(std::move(parser), parser->getDocument());
		});
	}

	std::shared_ptr<xercesc::DOMDocument> load_from_file(const std::string & file)
	{
		auto xfile = to_xmlch(file);
		return load_from_file(xfile);
	}

	std::shared_ptr<xercesc::DOMDocument> load_from_file(const xml_string & file)
	{
		return wrapped_load_xml([&file]
		{
			auto parser = std::make_shared<xercesc::XercesDOMParser>();
			xercesc_utils::HandlerBasePtr err(new xercesc::HandlerBase);
			xercesc::LocalFileInputSource input = file.c_str();

			parser->setDoNamespaces(true);
			parser->setErrorHandler(err.get());
			parser->parse(input);
			parser->setErrorHandler(nullptr);

			// aliasing constructor
			return std::shared_ptr<xercesc::DOMDocument>(std::move(parser), parser->getDocument());
		});
	}

	std::shared_ptr<xercesc::DOMDocument> load(std::streambuf & sb)
	{
		return wrapped_load_xml([&sb]
		{
			auto parser = std::make_shared<xercesc::XercesDOMParser>();
			xercesc_utils::HandlerBasePtr err(new xercesc::HandlerBase);

			streambuf_input_source input(&sb);

			parser->setDoNamespaces(true);
			parser->setErrorHandler(err.get());
			parser->parse(input);
			parser->setErrorHandler(nullptr);

			// aliasing constructor
			return std::shared_ptr<xercesc::DOMDocument>(std::move(parser), parser->getDocument());
		});
	}

	std::shared_ptr<xercesc::DOMDocument> load(std::istream & is)
	{
		auto * sbuf = is.rdbuf();
		if (not sbuf) throw std::logic_error("xercesc_utils::load: std::istream does not have streambuf!");
		return load(*sbuf);
	}


	void DOMXPathNSResolverImpl::addNamespaceBinding(xml_string prefix, xml_string uri)
	{
		m_prefix_mappings[uri] = prefix;
		m_uri_mappings[prefix] = uri;
	}

	void DOMXPathNSResolverImpl::addNamespaceBinding(const XMLCh * prefix, const XMLCh * uri)
	{
		m_prefix_mappings[uri] = prefix;
		m_uri_mappings[prefix] = uri;
	}

	const XMLCh * DOMXPathNSResolverImpl::lookupNamespaceURI(const XMLCh * prefix) const
	{
		auto it = m_uri_mappings.find(prefix);
		if (it == m_uri_mappings.end()) return nullptr;

		return it->second.c_str();
	}

	const XMLCh * DOMXPathNSResolverImpl::lookupPrefix(const XMLCh * uri) const
	{
		auto it = m_prefix_mappings.find(uri);
		if (it == m_prefix_mappings.end()) return nullptr;

		return it->second.c_str();
	}

	DOMXPathNSResolverImpl * DOMXPathNSResolverImpl::clone() const
	{
		auto * clone = new DOMXPathNSResolverImpl;
		clone->m_uri_mappings = m_uri_mappings;
		clone->m_prefix_mappings = m_prefix_mappings;

		return clone;
	}

	void DOMXPathNSResolverImpl::release()
	{
		delete this;
	}


	DOMXPathNSResolverImplPtr create_resolver(std::initializer_list<std::pair<std::string_view, std::string_view>> items)
	{
		DOMXPathNSResolverImplPtr resolver(new DOMXPathNSResolverImpl);
		for (auto & item : items)
			resolver->addNamespaceBinding(item.first, item.second);

		return resolver;
	}

	DOMXPathNSResolverImplPtr create_resolver(std::initializer_list<std::pair<xml_string, xml_string>> items)
	{
		DOMXPathNSResolverImplPtr resolver(new DOMXPathNSResolverImpl);
		for (auto & item : items)
			resolver->addNamespaceBinding(item.first, item.second);

		return resolver;
	}

	class CustomResolverDataHandler : public xercesc::DOMUserDataHandler
	{
	public:
		virtual void handle(DOMOperationType operation, const XMLCh * const key, void * data, const xercesc_3_2::DOMNode * src, xercesc_3_2::DOMNode * dst) override;
	};

	void CustomResolverDataHandler::handle(DOMOperationType operation, const XMLCh * const key, void * data, const xercesc_3_2::DOMNode * src, xercesc_3_2::DOMNode * dst)
	{
		if (not data) return;

		switch (operation)
		{
			case NODE_CLONED:
			{
				auto * resolver = static_cast<xercesc::DOMXPathNSResolver *>(data);
				if (auto * implr = dynamic_cast<DOMXPathNSResolverImpl *>(resolver))
					associate_custom_resolver(dst, DOMXPathNSResolverPtr(implr->clone()));
				return;
			}

			case NODE_IMPORTED:
			case NODE_RENAMED:
			case NODE_ADOPTED:
			default: return;

			case NODE_DELETED:
			{
				auto * resolver = static_cast<xercesc::DOMXPathNSResolver *>(data);
				resolver->release();
				return;
			}
		}
	}

	const xml_string CUSTOM_RESOLVER = XERCESC_LIT("xercesc_utils::custom_resolver");
	CustomResolverDataHandler g_custom_resolver_hanlder;

	void associate_custom_resolver(xercesc::DOMNode * node, DOMXPathNSResolverPtr resolver)
	{
		if (not node) throw std::invalid_argument("xercesc_utils::associate_custom_resolver: node is null");
		auto * prev = node->setUserData(CUSTOM_RESOLVER.c_str(), resolver.release(), &g_custom_resolver_hanlder);
		if (prev) static_cast<xercesc::DOMXPathNSResolver *>(prev)->release();
	}

	auto get_associated_custom_resolver(xercesc::DOMNode * node) -> xercesc::DOMXPathNSResolver *
	{
		return static_cast<xercesc::DOMXPathNSResolver *>(node->getUserData(CUSTOM_RESOLVER.c_str()));
	}

	auto associate_namespaces(xercesc::DOMNode * node, std::initializer_list<std::pair<std::string_view, std::string_view>> items) -> DOMXPathNSResolverImpl *
	{
		auto resolver = create_resolver(items);
		auto ret = resolver.get();
		associate_custom_resolver(node, std::move(resolver));
		return ret;
	}

	auto associate_namespaces(xercesc::DOMNode * node, std::initializer_list<std::pair<xml_string, xml_string>> items) -> DOMXPathNSResolverImpl *
	{
		auto resolver = create_resolver(items);
		auto ret = resolver.get();
		associate_custom_resolver(node, std::move(resolver));
		return ret;
	}

	void set_namespace(xercesc::DOMElement * element, xml_string prefix, xml_string uri)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::set_namespace: element is null");
		element->setAttributeNS(u"http://www.w3.org/2000/xmlns/", prefix.c_str(), uri.c_str());
	}

	void set_namespace(xercesc::DOMDocument * doc, xml_string prefix, xml_string uri)
	{
		set_namespace(doc->getDocumentElement(), prefix, uri);
	}

	void set_namespaces(xercesc::DOMDocument * doc, std::initializer_list<std::pair<std::string_view, std::string_view>> items)
	{
		for (auto & item : items)
			set_namespace(doc, item.first, item.second);
	}

	void set_namespaces(xercesc::DOMDocument * doc, std::initializer_list<std::pair<xml_string, xml_string>> items)
	{
		for (auto & item : items)
			set_namespace(doc, item.first, item.second);
	}


	namespace detail
	{
		/// сравнивает и идентифицирует отношение 2 аргументов
		/// -1 if op1 < op2;
		/// 0 if op1 == op2;
		/// +1 if op1 > op2;
		template <class Type>
		static int tribool_compare(const Type & op1, const Type & op2)
		{
			if (op1 < op2) return -1;
			if (op2 < op1) return +1;
			return 0;
		}

		static int compare(const XMLCh * str1_first, const XMLCh * str1_last,
		                   const XMLCh * str2_first, const XMLCh * str2_last)
		{
			auto n1 = str1_last - str1_first;
			auto n2 = str2_last - str2_first;
			int res = std::char_traits<XMLCh>::compare(str1_first, str2_first, (std::min)(n1, n2));
			if (res != 0) return res;

			//strings equal, length determines result, compare n1 <=> n2;
			return tribool_compare(n1, n2);
		}

		template <class Iterator>
		inline static Iterator skip_separartors(Iterator first)
		{
			while (*first == separator) ++first;
			return first;
		}

		inline static bool is_space(XMLCh ch)
		{
			return ch == ' ' || ch == '\r' || ch == '\n';
		}

		[[noreturn]] static void throw_prefix_not_found(const XMLCh * prefix)
		{
			std::string errmsg;
			errmsg.reserve(64);
			errmsg = "xml namespace not found, prefix = ";
			errmsg += to_utf8(prefix);
			throw xml_namespace_exception(errmsg);
		}

		static xml_string_view lookupNamespaceURI(const xercesc::DOMXPathNSResolver * resolver, const XMLCh * prefix)
		{
			auto uri = resolver->lookupNamespaceURI(prefix);
			if (not uri) throw_prefix_not_found(prefix);
			return uri == nullptr ? XERCESC_LIT("") : uri;
		}

		static xml_string_view lookupNamespaceURI(const xercesc::DOMNode * node, const XMLCh * prefix)
		{
			auto uri = node->lookupNamespaceURI(prefix);
			if (not uri) throw_prefix_not_found(prefix);
			return uri;
		}

		static xml_string_view getNamespaceURI(const xercesc::DOMNode * node)
		{
			auto uri = node->getNamespaceURI();
			return uri == nullptr ? XERCESC_LIT("") : uri;
		}


		static xml_string_view get_text_content(xercesc::DOMElement * element)
		{
			auto first = element->getTextContent();
			auto last = first + xercesc::XMLString::stringLen(first);

			typedef std::reverse_iterator<const XMLCh *> reverse_it;

			first = std::find_if_not(first, last, is_space);
			last = std::find_if_not(reverse_it(last), reverse_it(first), is_space).base();

			return xml_string_view(first, last - first);
		}

		static xercesc::DOMElement * find_root(xercesc::DOMDocument * doc, xml_string_view name)
		{
			xercesc::DOMElement * root = doc->getDocumentElement();
			if (not root) return nullptr;

			auto * resolver = get_associated_custom_resolver(doc);

			auto * first = name.data();
			auto * last  = first + name.size();
			xml_string nsprefix;
			xml_string_view searched_ns;

			auto nspos = name.find(':');
			if (nspos != name.npos)
			{   // exists namespace
				nsprefix.assign(first, first + nspos);
				first += nspos + 1;
				searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
				                       : lookupNamespaceURI(root, nsprefix.c_str());
			}

			xml_string_view local_ns = getNamespaceURI(root);
			if (local_ns != searched_ns) return nullptr;

			auto * name_first = root->getLocalName();
			if (not name_first) name_first = root->getNodeName();
			auto * name_last = name_first + std::char_traits<XMLCh>::length(name_first);

			if (compare(first, last, name_first, name_last) == 0)
				return root;

			return nullptr;
		}

		static xercesc::DOMElement * acquire_root(xercesc::DOMDocument * doc, xml_string_view name)
		{
			assert(doc);
			auto * resolver = get_associated_custom_resolver(doc);
			xercesc::DOMElement * root = doc->getDocumentElement();

			auto * first = name.data();
			auto * last  = first + name.size();
			xml_string nsprefix;
			xml_string_view searched_ns;

			auto nspos = name.find(':');
			if (nspos != name.npos)
			{   // exists namespace
				nsprefix.assign(first, first + nspos);
				first += nspos + 1;
				searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
				            : root     ? lookupNamespaceURI(root, nsprefix.c_str())
				            :            lookupNamespaceURI(doc,  nsprefix.c_str());
			}

			if (root)
			{
				// if empty root asked return any already existing root
				if (first == last) return root;

				xml_string_view local_ns = getNamespaceURI(root);
				if (local_ns != searched_ns)
				{
					std::string err_msg = "xercesc_utils::acquire_root: document already has root node and it's xml namespace is different";
					err_msg += "has = "; err_msg += xercesc_utils::to_utf8(local_ns);
					err_msg += ", asked = "; err_msg.append(searched_ns.begin(), searched_ns.end());

					throw std::runtime_error(err_msg);
				}
				else
				{
					auto * name_first = root->getLocalName();
					if (not name_first) name_first = root->getNodeName();
					auto * name_last = name_first + std::char_traits<XMLCh>::length(name_first);

					if (compare(first, last, name_first, name_last) == 0)
						return root;
					else
					{
						std::string err_msg = "xercesc_utils::acquire_root: document already has root node and it's name is different";
						err_msg += "has = "; err_msg += xercesc_utils::to_utf8(name_first, name_last - name_first);
						err_msg += ", asked = "; err_msg.append(first, last);

						throw std::runtime_error(err_msg);
					}
				}

			}
			else
			{
				try
				{
					root = doc->createElementNS(searched_ns.data(), name.data());
					//root->setPrefix(nsprefix.c_str());
					doc->appendChild(root);
					return root;
				}
				catch (xercesc::DOMException & ex)
				{
					auto err = xercesc_utils::to_utf8(ex.getMessage());
					std::throw_with_nested(std::runtime_error(std::move(err)));
				}
			}
		}
	} // namespace detail

	xercesc::DOMElement * find_child(xercesc::DOMElement * element, xml_string_view name)
	{
		if (not element) return nullptr;
		using namespace detail;

		auto * doc = element->getOwnerDocument();
		auto * resolver = get_associated_custom_resolver(doc);

		auto * first = name.data();
		auto * last  = first + name.size();
		xml_string nsprefix;
		xml_string_view searched_ns;

		auto nspos = name.find(':');
		if (nspos != name.npos)
		{   // exists namespace
			nsprefix.assign(first, first + nspos);
			first += nspos + 1;
			searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
			                       : lookupNamespaceURI(element,  nsprefix.c_str());
		}

		for (auto * node = element->getFirstElementChild(); node; node = node->getNextElementSibling())
		{
			xml_string_view local_ns = getNamespaceURI(node);
			if (local_ns != searched_ns) continue;

			auto name_first = node->getLocalName();
			if (not name_first) name_first = node->getNodeName();
			auto * name_last = name_first + std::char_traits<XMLCh>::length(name_first);

			if (compare(name_first, name_last, first, last) == 0)
				return node;
		}

		return nullptr;
	}

	xercesc::DOMElement * get_child(xercesc::DOMElement * element, xml_string_view name)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_child: element is null");
		element = find_child(element, name);

		if (not element) throw xml_path_exception(name);
		return element;
	}

	xercesc::DOMElement * find_path(xercesc::DOMElement * element, xml_string_view path)
	{
		using namespace detail;

		auto * first = path.data();
		auto * last  = first + path.size();

		if (element == nullptr || first == last) return element;

		if (*first == separator)
		{
			return find_path(element->getOwnerDocument(), path);
		}

		decltype(first) next;
		while (first != last)
		{
			next = std::find(first, last, separator);
			element = find_child(element, xml_string_view(first, next - first));
			if (!element) return nullptr;

			first = skip_separartors(next);
		}

		return element;
	}

	xercesc::DOMElement * find_path(xercesc::DOMDocument * doc, xml_string_view path)
	{
		using namespace detail;

		auto * first = path.data();
		auto * last  = first + path.size();

		if (doc == nullptr || first == last) return nullptr;

		first = skip_separartors(first);
		auto next = std::find(first, last, separator);
		auto * element = find_root(doc, xml_string_view(first, next - first));

		next = skip_separartors(next);
		return find_path(element, xml_string_view(next, last - next));
	}

	xercesc::DOMElement * get_path(xercesc::DOMElement * element, xml_string_view path)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_path: element is null");
		element = find_path(element, path);

		if (not element) throw xml_path_exception(path);
		return element;
	}

	xercesc::DOMElement * get_path(xercesc::DOMDocument * doc, xml_string_view path)
	{
		if (not doc) throw std::invalid_argument("xercesc_utils::get_path: document is null");
		auto * element = find_path(doc, path);

		if (not element) throw xml_path_exception(path);
		return element;
	}

	xercesc::DOMElement * acquire_path(xercesc::DOMElement * node, xml_string path)
	{
		using namespace detail;

		if (path.empty()) return node;

		if (not node) throw std::invalid_argument("xercesc_utils::acquire_path: node is null");
		auto * doc = node->getOwnerDocument();

		if (path.front() == separator)
			return acquire_path(doc, std::move(path));

		auto first = &path[0];
		auto last = first + path.size();
		// last actually points to valid writable location, with string nul terminator

		for (;;)
		{
			auto next = std::find(first, last, separator);
			*next = 0;

			auto * fnode = find_path(node, xml_string_view(first, next - first));
			if (not fnode)
			{
				auto * resolver = get_associated_custom_resolver(doc);
				xml_string nsprefix;
				xml_string_view searched_ns;

				auto it = std::find(first, last, ':');
				if (it != last)
				{   // exists namespace
					nsprefix.assign(first, it);
					searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
					                       : lookupNamespaceURI(node,     nsprefix.c_str());
				}

				try
				{
					fnode = doc->createElementNS(searched_ns.data(), first);
					node->appendChild(fnode);
				}
				catch (xercesc::DOMException & ex)
				{
					auto err = xercesc_utils::to_utf8(ex.getMessage());
					std::throw_with_nested(std::runtime_error(std::move(err)));
				}
			}

			node = fnode;
			if (next == last) break;
			first = skip_separartors(++next);
			if (first == last) break;
		}

		return node;
	}

	xercesc::DOMElement * acquire_path(xercesc::DOMDocument * doc, xml_string path)
	{
		using namespace detail;

		if (not doc) throw std::invalid_argument("xercesc_utils::acquire_path: document is null");

		auto first = &path[0];
		auto last = first + path.size();
		// last actually points to valid writable location, with string nul terminator

		first = skip_separartors(first);
		auto next = std::find(first, last, separator);
		*next = 0;

		auto * node = acquire_root(doc, xml_string_view(first, next - first));
		if (next == last) return node;

		for (first = skip_separartors(++next); first != last; first = skip_separartors(++next))
		{
			next = std::find(first, last, separator);
			*next = 0;

			auto * fnode = find_path(node, xml_string_view(first, next - first));
			if (not fnode)
			{
				auto * resolver = get_associated_custom_resolver(doc);
				xml_string nsprefix;
				xml_string_view searched_ns;

				auto it = std::find(first, last, ':');
				if (it != last)
				{   // exists namespace
					nsprefix.assign(first, it);
					searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
					                       : lookupNamespaceURI(node,     nsprefix.c_str());
				}

				try
				{
					fnode = doc->createElementNS(searched_ns.data(), first);
					node->appendChild(fnode);
				}
				catch (xercesc::DOMException & ex)
				{
					auto err = xercesc_utils::to_utf8(ex.getMessage());
					std::throw_with_nested(std::runtime_error(std::move(err)));
				}
			}

			node = fnode;
			if (next == last) break;
		}

		return node;
	}

	std::string get_text_content(xercesc::DOMElement * element)
	{
		return to_utf8(detail::get_text_content(element));
	}

	void set_text_content(xercesc::DOMElement * element, std::string_view text)
	{
		element->setTextContent(to_xmlch(text).c_str());
	}

	std::string find_path_text(xercesc::DOMDocument * doc, xml_string_view path, std::string_view defval /*= empty_string*/)
	{
		auto * element = find_path(doc, path);
		if (not element) return std::string(defval.data(), defval.size());

		return get_text_content(element);
	}

	std::string find_path_text(xercesc::DOMElement * element, xml_string_view path, std::string_view defval /*= empty_string*/)
	{
		element = find_path(element, path);
		if (not element) return std::string(defval.data(), defval.size());

		return get_text_content(element);
	}

	std::string get_path_text(xercesc::DOMDocument * doc, xml_string_view path)
	{
		if (not doc) throw std::invalid_argument("xercesc_utils::get_path_text: document is null");
		auto * element = find_path(doc, path);
		if (not element) throw xml_path_exception(path);

		return get_text_content(element);
	}

	std::string get_path_text(xercesc::DOMElement * element, xml_string_view path)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_path_text: element is null");
		element = find_path(element, path);
		if (not element) throw xml_path_exception(path);

		return get_text_content(element);
	}

	void set_path_text(xercesc::DOMDocument * doc, xml_string path, std::string_view value)
	{
		if (not doc) throw std::invalid_argument("xercesc_utils::set_path_text: document is null");
		auto * node = acquire_path(doc, std::move(path));

		auto val = to_xmlch(value);
		node->setTextContent(val.c_str());
	}

	void set_path_text(xercesc::DOMElement * elem, xml_string path, std::string_view value)
	{
		if (not elem) throw std::invalid_argument("xercesc_utils::set_path_text: element is null");
		elem = acquire_path(elem, std::move(path));

		auto val = to_xmlch(value);
		elem->setTextContent(val.c_str());
	}

	xercesc::DOMAttr * find_attribute_node(xercesc::DOMElement * element, const xml_string & attrname)
	{
		if (not element) return nullptr;
		using namespace detail;

		auto * doc = element->getOwnerDocument();
		auto * resolver = get_associated_custom_resolver(doc);

		auto * first = attrname.data();
		auto * last  = first + attrname.size();
		xml_string nsprefix;
		xml_string_view searched_ns;

		auto it = std::find(first, last, ':');
		if (it != last)
		{   // exists namespace
			nsprefix.assign(first, it);
			searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
			                       : lookupNamespaceURI(element,  nsprefix.c_str());

			return element->getAttributeNodeNS(searched_ns.data(), ++it);
		}
		else
			return element->getAttributeNode(first);
	}

	xercesc::DOMAttr *  get_attribute_node(xercesc::DOMElement * element, const xml_string & attrname)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_attribute_node: element is null");
		return find_attribute_node(element, attrname);
	}

	std::string find_attribute_text(xercesc::DOMElement * element, const xml_string & attrname, std::string_view defval /*= empty_string*/)
	{
		auto * attr = find_attribute_node(element, attrname);
		if (not attr) return std::string(defval.data(), defval.size());

		return to_utf8(attr->getValue());
	}

	std::string  get_attribute_text(xercesc::DOMElement * element, const xml_string & attrname)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_attribute_text: element is null");
		auto * attr = find_attribute_node(element, attrname);

		if (not attr) throw std::runtime_error("xml attribute \"" + to_utf8(attrname) + "\" not found");

		return to_utf8(attr->getValue());

	}

	void set_attribute_text(xercesc::DOMElement * element, const xml_string & attrname, std::string_view text)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::set_attribute_text: element is null");
		using namespace detail;

		auto * doc = element->getOwnerDocument();
		auto * resolver = get_associated_custom_resolver(doc);

		auto * first = attrname.data();
		auto * last  = first + attrname.size();
		xml_string nsprefix;
		xml_string_view searched_ns;

		auto it = std::find(first, last, ':');
		if (it != last)
		{   // exists namespace
			nsprefix.assign(first, it);
			searched_ns = resolver ? lookupNamespaceURI(resolver, nsprefix.c_str())
			                       : lookupNamespaceURI(element,  nsprefix.c_str());
		}

		try
		{
			if (it == last)
				element->setAttribute(first, to_xmlch(text).c_str());
			else
				element->setAttributeNS(searched_ns.data(), first, to_xmlch(text).c_str());
		}
		catch (xercesc::DOMException & ex)
		{
			auto err = xercesc_utils::to_utf8(ex.getMessage());
			std::throw_with_nested(std::runtime_error(std::move(err)));
		}
	}


	static xml_string prefix_name(xml_string_view node_name, xml_string_view prefix)
	{
		xml_string name;
		if (prefix.empty())
		{
			name.assign(node_name);
			return name;
		}
		else
		{
			name.reserve(prefix.size() + 1 + node_name.length());
			name.append(prefix);
			name.append(1, XERCESC_LIT(':'));
			name.append(node_name);
			return name;
		}
	}


	xercesc::DOMElement * rename_subtree(xercesc::DOMElement * element, const xml_string & namespace_uri, const xml_string & prefix)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::rename_subtree: element is null");

		auto * doc = element->getOwnerDocument();
		for (auto * node = element->getFirstChild(); node; node = node->getNextSibling())
		{
			auto type = node->getNodeType();
			if (type == node->ELEMENT_NODE)
				rename_subtree(static_cast<xercesc::DOMElement *>(node), namespace_uri, prefix);
			else if (type == node->ATTRIBUTE_NODE)
			{
				auto * attr = static_cast<xercesc::DOMAttr *>(node);
				xml_string attr_name = prefix_name(node->getNodeName(), prefix);
				doc->renameNode(attr, namespace_uri.data(), attr_name.c_str());
			}
		}

		xml_string node_name = prefix_name(element->getNodeName(), prefix);
		return static_cast<xercesc::DOMElement *>(doc->renameNode(element, namespace_uri.data(), node_name.c_str()));
	}

	xercesc::DOMNode * rename_subtree(xercesc::DOMNode * node, const xml_string & namespace_uri, const xml_string & prefix)
	{
		if (not node) throw std::invalid_argument("xercesc_utils::rename_subtree: node is null");

		auto type = node->getNodeType();
		if (type == node->ELEMENT_NODE)
			return rename_subtree(node, namespace_uri, prefix);
		else if (type == node->ATTRIBUTE_NODE)
		{
			auto * doc = node->getOwnerDocument();
			auto * attr = static_cast<xercesc::DOMAttr *>(node);
			xml_string attr_name = prefix_name(node->getNodeName(), prefix);
			return doc->renameNode(attr, namespace_uri.data(), attr_name.c_str());
		}

		return node;
	}

	xercesc::DOMElement * find_xpath(xercesc::DOMElement * element, const xml_string & path)
	{
		if (not element) return nullptr;
		auto * doc = element->getOwnerDocument();

		auto * resolver = get_associated_custom_resolver(doc);
		if (resolver)
			return find_xpath(element, path, resolver);
		else
		{
			DOMXPathNSResolverPtr resolver (doc->createNSResolver(element));
			return find_xpath(element, path, resolver.get());
		}
	}

	xercesc::DOMElement * find_xpath(xercesc::DOMElement * element, const xml_string & path, xercesc::DOMXPathNSResolver * resolver)
	{
		if (not element) return nullptr;
		auto * doc = element->getOwnerDocument();

		try
		{
			auto * result = doc->evaluate(
				path.c_str(), doc->getDocumentElement(), resolver,
				xercesc::DOMXPathResult::ANY_UNORDERED_NODE_TYPE, nullptr);

			if (not result) return nullptr;

			auto * resnode = result->getNodeValue();
			if (not resnode) return nullptr;

			if (resnode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
				return nullptr;

			return static_cast<xercesc::DOMElement *>(resnode);
		}
		catch (xercesc::DOMException & ex)
		{
			auto err = xercesc_utils::to_utf8(ex.getMessage());
			std::throw_with_nested(std::runtime_error(std::move(err)));
		}
	}


	xercesc::DOMElement * get_xpath(xercesc::DOMElement * element, const xml_string & path)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_xpath: element is null");
		element = find_xpath(element, path);

		if (not element) throw xml_path_exception(path);
		return element;
	}

	xercesc::DOMElement * get_xpath(xercesc::DOMElement * element, const xml_string & path, xercesc::DOMXPathNSResolver * resolver)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_xpath: element is null");
		element = find_xpath(element, path, resolver);
		if (not element) throw xml_path_exception(path);
		return element;
	}

	std::string find_xpath_text(xercesc::DOMElement * element, const xml_string & path, std::string_view defval /*= empty_string*/)
	{
		element = find_xpath(element, path);
		if (not element) return std::string(defval.data(), defval.size());
		return get_text_content(element);
	}

	std::string get_xpath_text(xercesc::DOMElement * element, const xml_string & path)
	{
		if (not element) throw std::invalid_argument("xercesc_utils::get_xpath_text: element is null");
		element = find_xpath(element, path);
		if (not element) throw xml_path_exception(path);
		return get_text_content(element);
	}
}

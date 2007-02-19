#include "Reflex/Type.h"
#include "Reflex/Member.h"
#include "Reflex/Base.h"
#include "boost/thread/tss.hpp"
#include <set>
#include <algorithm>

#include "DataFormats/Common/interface/ReflexTools.h"
#include "DataFormats/Common/interface/Wrapper.h"

namespace ROOT {
  namespace Reflex {
    std::ostream& operator<< (std::ostream& os, Type const& t) {
      os << t.Name();
      return os;
    }

    std::ostream& operator<< (std::ostream& os, TypeTemplate const& tt) {
      os << tt.Name();
      return os;
    }
  }
}

using ROOT::Reflex::Type;
using ROOT::Reflex::Type_Iterator;
using ROOT::Reflex::Member;
using ROOT::Reflex::SCOPED;


namespace edm
{
  
  bool 
  find_nested_type_named(std::string const& nested_type,
			 Type const& type_to_search,
			 Type& found_type)
  {
    // Look for a sub-type named 'nested_type'
    for (Type_Iterator
	   i = type_to_search.SubType_Begin(),
	   e = type_to_search.SubType_End();
	 i != e;
	 ++i)
      {
	if (i->Name() == nested_type)
	  {
	    found_type = i->ToType();
	    return true;
	  }
      }
    return false;
  }

  bool
  is_sequence_wrapper(Type const& possible_sequence_wrapper,
		      Type& found_sequence_value_type)
  {
    Type possible_sequence;
    return 
      wrapper_type_of(possible_sequence_wrapper, possible_sequence) &&
      value_type_of(possible_sequence, found_sequence_value_type);
  }

  bool
  checkDictionary(Type c) {
    while (c.IsPointer() == true || c.IsArray() == true || c.IsTypedef() == true || c.IsReference() == true) {
      c = c.ToType();
    }
    if (c.IsFundamental()) return false;
    if (c.IsEnum()) return false;
    std::string name = c.Name(SCOPED);
    if (name.empty()) return false;
    Type t = Type::ByName(name);
    if (!bool(t)) {
      throw edm::Exception(edm::errors::DictionaryNotFound)
        << "No REFLEX data dictionary found for class '" << name << "'.\n"
        << "Most likely this dictionary was never generated,\n"
        << "but it may be that it was generated in the wrong package.\n"
	<< "Please add (or move) the specification\n"
	<< "<class name=\"" << name << "\"/>\n"
	<< "to the appropriate classes_def.xml file.\n"
	<< "If the class is a template instance, you may need\n"
	<< "to define a dummy variable of this type in classes.h.\n"
	<< "Also, if this class has any transient members,\n"
	<< "you need to specify them in classes_def.xml.";
    }
    return true;
  }

  namespace {
    int const oneParamArraySize = 6;
    std::string const oneParam[oneParamArraySize] = {
      "vector",
      "basic_string",
      "set",
      "list",
      "deque",
      "multiset"
    };
    int const twoParamArraySize = 3;
    std::string const twoParam[twoParamArraySize] = {
      "map",
      "pair",
      "multimap"
    };
  }

  void
  checkDictionaries(Type t) {
    // The only purpose of this cache is to stop infinite recursion.
    // Reflex maintains its own internal cache.
    typedef std::set<std::string> Set;
    static boost::thread_specific_ptr<Set> s_types;
    if(0 == s_types.get()){
      s_types.reset(new Set);
    }

    while(t.IsPointer() == true || t.IsArray() == true || t.IsTypedef() == true || t.IsReference() == true) {
      t = t.ToType();
    }

    std::string name = t.Name(SCOPED);

    if(s_types->end() != s_types->find(name)) {
	// Already been processed
	return;
    }
    s_types->insert(name);

    if (!checkDictionary(t)) return;

    if(name.find("std::") == 0) {
      if (t.IsTemplateInstance()) {
	std::string::size_type n = name.find('<');
	int cnt = 0;
	if (std::find(oneParam, oneParam + oneParamArraySize, name.substr(5, n - 5)) != oneParam + oneParamArraySize) {
	  cnt = 1;
	} else if (std::find(twoParam, twoParam + twoParamArraySize, name.substr(5, n - 5)) != twoParam + twoParamArraySize) {
	  cnt = 2;
	} 
        for(int i = 0; i < cnt; ++i) {
          checkDictionaries(t.TemplateArgumentAt(i));
        }
      }
    } else {
      int mcnt = t.DataMemberSize();
      for(int i = 0; i < mcnt; ++i) {
        Member m = t.DataMemberAt(i);
        if(m.IsTransient() || m.IsStatic()) continue;
        checkDictionaries(m.TypeOf());
      }
      int cnt = t.BaseSize();
      for(int i = 0; i < cnt; ++i) {
        checkDictionaries(t.BaseAt(i).ToType());
      }
    }
  }

  void checkDictionaries(std::string const& name, bool transient) {
    if (transient) {
      checkDictionary(Type::ByName(name));
    } else {
      checkDictionary(Type::ByName(wrappedClassName(name)));
      checkDictionaries(Type::ByName(name));
    }
  }
}

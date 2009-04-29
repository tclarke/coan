from ctypes import *
from ctypes import util
raptor = cdll.LoadLibrary(util.find_library("raptor"))

RAPTOR_FEATURE_SCANNING=0
RAPTOR_FEATURE_ASSUME_IS_RDF=1
RAPTOR_FEATURE_ALLOW_NON_NS_ATTRIBUTES=2
RAPTOR_FEATURE_ALLOW_OTHER_PARSETYPES=3
RAPTOR_FEATURE_ALLOW_BAGID=4
RAPTOR_FEATURE_ALLOW_RDF_TYPE_RDF_LIST=5
RAPTOR_FEATURE_NORMALIZE_LANGUAGE=6
RAPTOR_FEATURE_NON_NFC_FATAL=7
RAPTOR_FEATURE_WARN_OTHER_PARSETYPES=8
RAPTOR_FEATURE_CHECK_RDF_ID=9
RAPTOR_FEATURE_RELATIVE_URIS=10
RAPTOR_FEATURE_START_URI=11
RAPTOR_FEATURE_WRITER_AUTO_INDENT=12
RAPTOR_FEATURE_WRITER_AUTO_EMPTY=13
RAPTOR_FEATURE_WRITER_INDENT_WIDTH=14
RAPTOR_FEATURE_WRITER_XML_VERSION=15
RAPTOR_FEATURE_WRITER_XML_DECLARATION=16
RAPTOR_FEATURE_NO_NET=17
RAPTOR_FEATURE_RESOURCE_BORDER=18
RAPTOR_FEATURE_LITERAL_BORDER=19
RAPTOR_FEATURE_BNODE_BORDER=20
RAPTOR_FEATURE_RESOURCE_FILL=21
RAPTOR_FEATURE_LITERAL_FILL=22
RAPTOR_FEATURE_BNODE_FILL=23
RAPTOR_FEATURE_HTML_TAG_SOUP=24
RAPTOR_FEATURE_MICROFORMATS=25
RAPTOR_FEATURE_HTML_LINK=26
RAPTOR_FEATURE_WWW_TIMEOUT=27
RAPTOR_FEATURE_LAST=17

RAPTOR_IDENTIFIER_TYPE_UNKNOWN=0
RAPTOR_IDENTIFIER_TYPE_RESOURCE=1
RAPTOR_IDENTIFIER_TYPE_ANONYMOUS=2
RAPTOR_IDENTIFIER_TYPE_PREDICATE=3
RAPTOR_IDENTIFIER_TYPE_ORDINAL=4
RAPTOR_IDENTIFIER_TYPE_LITERAL=5
RAPTOR_IDENTIFIER_TYPE_XML_LITERAL=6

class raptor_statement(Structure):
   _fields_ = [("subject", c_void_p),
               ("subject_type", c_int),
               ("predicate", c_void_p),
               ("predicate_type", c_int),
               ("object", c_void_p),
               ("object_type", c_int),
               ("object_literal_datatype", c_void_p),
               ("object_literal_language", POINTER(c_ubyte))]

class raptor_locator(Structure):
   _fields_ = [("uri", c_void_p),
               ("file", c_char_p),
               ("line", c_int),
               ("column", c_int),
               ("byte", c_int)]

STATEMENT_FUNC = CFUNCTYPE(None, c_void_p, POINTER(raptor_statement))
HANDLER_FUNC = CFUNCTYPE(None, c_void_p, POINTER(raptor_locator), c_char_p)

@HANDLER_FUNC
def handle_raptor_error(user, locator, message):
   locator = locator.contents
   print "ERROR line %i column %i: %s" % (locator.line, locator.column, message)

@HANDLER_FUNC
def handle_raptor_warning(user, locator, message):
   locator = locator.contents
   print "WARNING line %i column %i: %s" % (locator.line, locator.column, message)


class RaptorParser(object):
   tmp_map = None

   def __init__(self, parserName="guess"):
      raptor.raptor_init()
      self.parser = raptor.raptor_new_parser(parserName)
      raptor.raptor_set_statement_handler(self.parser, None, self.__parse_raptor_triple)
      raptor.raptor_set_error_handler(self.parser, None, handle_raptor_error)
      raptor.raptor_set_warning_handler(self.parser, None, handle_raptor_warning)

   def cleanup(self):
      raptor.raptor_free_parser(self.parser)
      raptor.raptor_finish()

   def set_feature(self, feature, value=1):
      raptor.raptor_set_feature(self.parser, feature, value)

   def parse_file(self, filename):
      RaptorParser.tmp_map = {}
      uristr = raptor.raptor_uri_filename_to_uri_string(filename)
      uri = raptor.raptor_new_uri(uristr)
      rval = raptor.raptor_parse_file(self.parser, uri, None)
      raptor.raptor_free_uri(uri)
      raptor.raptor_free_memory(uristr)
      self.__statements = RaptorParser.tmp_map
      RaptorParser.tmp_map = None
      return rval

   def __repr__(self):
      if self.__statements is None:
         return "<Rdf: Unparsed>"
      return "<Rdf: %i subjects>" % len(self.__statements)

   def statements(self):
      return self.__statements

   @STATEMENT_FUNC
   def __parse_raptor_triple(stlist, statement):
      statement = statement.contents
      subject = cast(raptor.raptor_statement_part_as_string(statement.subject, statement.subject_type, None, None), c_char_p).value
      predicate = cast(raptor.raptor_statement_part_as_string(statement.predicate, statement.predicate_type, None, None), c_char_p).value
      object = cast(raptor.raptor_statement_part_as_string(statement.object, statement.object_type, None, None), c_char_p).value
      if subject[0] == "<" and subject[-1] == ">": subject = subject[1:-1]
      if predicate[0] == "<" and predicate[-1] == ">": predicate = predicate[1:-1]
      if object[0] == "<" and object[-1] == ">": object = object[1:-1]

      if subject not in RaptorParser.tmp_map: RaptorParser.tmp_map[subject] = {}
      if predicate not in RaptorParser.tmp_map[subject]: RaptorParser.tmp_map[subject][predicate] = []
      RaptorParser.tmp_map[subject][predicate].append(object)

class RaptorSerializer(object):
   def __init__(self, serializerName="ntriples"):
      self.__statements = {}
      raptor.raptor_init()
      self.serializer = raptor.raptor_new_serializer(serializerName)
      raptor.raptor_set_error_handler(self.serializer, None, handle_raptor_error)
      raptor.raptor_set_warning_handler(self.serializer, None, handle_raptor_warning)

   def cleanup(self):
      raptor.raptor_free_serializer(self.serializer)
      raptor.raptor_finish()

   def set_feature(self, feature, value=1):
      raptor.raptor_serialize_set_feature(self.serializer, feature, value)

   def add_namespace(self, prefix, uri):
      ruri = raptor.raptor_new_uri(uri)
      raptor.raptor_serialize_set_namespace(self.serializer, prefix, ruri)
      raptor.raptor_free_uri(ruri)

   def serialize_to_file(self, filename):
      raptor.raptor_serialize_start_to_filename(self.serializer, filename)
      self.__serialize_statements()
      raptor.raptor_serialize_end(self.serializer)

   def serialize_to_string(self, base_uri = ""):
      ruri = raptor.raptor_new_uri(base_uri)
      outstr = c_void_p()
      outlen = c_ulong()
      raptor.raptor_serialize_start_to_string(self.serializer, ruri, pointer(outstr), pointer(outlen))
      self.__serialize_statements()
      raptor.raptor_serialize_end(self.serializer)
      return cast(outstr, c_char_p).value

   def __serialize_statements(self):
      for subject in iter(self.__statements):
         for predicate in iter(self.__statements[subject]):
            for object in iter(self.__statements[subject][predicate]):
               subject = subject.strip()
               predicate = predicate.strip()
               object = object.strip()
               if subject[0] == '"' and subject[-1] == '"': subject = subject[1:-1]
               if predicate[0] == '"' and predicate[-1] == '"': predicate = predicate[1:-1]
               if object[0] == '"' and object[-1] == '"': object = object[1:-1]
               triple = raptor_statement()
               if subject.startswith("_:"):
                  triple.subject = cast(c_char_p(subject), c_void_p)
                  triple.subject_type = RAPTOR_IDENTIFIER_TYPE_ANONYMOUS
               else:
                  triple.subject = raptor.raptor_new_uri(subject)
                  triple.subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE
               triple.predicate = raptor.raptor_new_uri(predicate)
               triple.predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE
               triple.object = cast(c_char_p(object), c_void_p)
               if object.startswith("_:"):
                  triple.object_type = RAPTOR_IDENTIFIER_TYPE_ANONYMOUS
               else:
                  triple.object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL
               triple.object_literal_datatype = None
               triple.object_literal_language = cast(c_char_p("en"), POINTER(c_ubyte))
               raptor.raptor_serialize_statement(self.serializer, pointer(triple))
               if triple.subject_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE:
                  raptor.raptor_free_uri(triple.subject)
               raptor.raptor_free_uri(triple.predicate)

   def statements(self, st):
      self.__statements = st

if __name__ == '__main__':
   r=RaptorParser()
   r.set_feature(RAPTOR_FEATURE_NO_NET)
   r.parse_file("install.n3")
   s = r.statements()
   r.cleanup()

   r=RaptorSerializer("rdfxml-abbrev")
   r.statements(s)
   s = r.serialize_to_string()
   print type(s)
   print s
   r.cleanup()

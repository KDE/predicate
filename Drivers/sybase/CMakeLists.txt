include_directories(
  ${FreeTDS_INCLUDE_DIR}
  ${Predicate_SOURCE_DIR}
  ${QT_INCLUDES}
)

ADD_DEFINITIONS(
  -D__PREDICATE__=
  -DPREDICATE_DRIVER_NAME="sybase"
)

########### next target ###############

set(predicate_sybase_SRCS 
   SybaseDriver.cpp 
   SybaseConnection.cpp 
   SybaseConnection_p.cpp 
   SybaseCursor.cpp 
   SybaseKeywords.cpp 
   SybasePreparedStatement.cpp )


add_library(predicate_sybase SHARED ${predicate_sybase_SRCS})

target_link_libraries(predicate_sybase
  ${FreeTDS_LIBRARIES}
  predicate
)
set_target_properties(predicate_sybase PROPERTIES PREFIX "")

install(TARGETS predicate_sybase DESTINATION ${PREDICATE_PLUGINS_DIR})
install(FILES predicate_sybase.desktop DESTINATION ${PREDICATE_PLUGINS_DIR})

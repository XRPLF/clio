add_library(clio_etlng INTERFACE)

# target_sources(clio_etlng PRIVATE )

target_link_libraries(clio_etlng INTERFACE clio_data)

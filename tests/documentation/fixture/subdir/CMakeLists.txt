fcg_register_documentation(
    LIBRARY First LINK_TARGET Fixture::First
    COMPONENTS fixture_first
    INPUTS "../public headers" "../extra/extra.hxx"
    EXAMPLE_DIRS "../snippets"
    PREDEFINED FIXTURE_EXPORT=
)

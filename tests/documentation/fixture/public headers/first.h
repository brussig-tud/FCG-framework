/** \defgroup fixture_first First module
 * \ingroup fcg_modules
 * \snippet sample.cpp fixture_example
 */
/** \ingroup fixture_first */
struct FIXTURE_EXPORT FirstApi {
    void undocumentedPublicMember();
private:
    void hiddenPrivateMember();
};
/** \defgroup fixture_mobile Mobile module
 * \ingroup fcg_modules
 * Its group identity survives library moves.
 */
/** \ingroup fixture_mobile */
struct MobileApi {};

/** \page fixture_library_overview Fixture library overview
 * Authored overview for the fixture library.
 */

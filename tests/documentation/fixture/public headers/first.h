/** \defgroup fixture_first First component
 * \ingroup fcg_components
 * \snippet sample.cpp fixture_example
 */
/** \ingroup fixture_first */
struct FIXTURE_EXPORT FirstApi {
    void undocumentedPublicMember();
private:
    void hiddenPrivateMember();
};
/** \defgroup fixture_mobile Mobile component
 * \ingroup fcg_components
 * Its group identity survives library moves.
 */
/** \ingroup fixture_mobile */
struct MobileApi {};

/** \page fixture_library_guide Fixture library guide
 * Authored guide for the fixture library.
 */

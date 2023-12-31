import pytest

pytestmark = pytest.mark.asyncio

from .. import get_document_focus, get_visibility_state


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_background_default_false(bidi_session, type_hint):
    new_context = await bidi_session.browsing_context.create(type_hint=type_hint)

    try:
        assert await get_visibility_state(bidi_session, new_context) == "visible"
        assert await get_document_focus(bidi_session, new_context) is True
    finally:
        await bidi_session.browsing_context.close(context=new_context["context"])


@pytest.mark.parametrize("type_hint", ["tab", "window"])
@pytest.mark.parametrize("background", [True, False])
async def test_background(bidi_session, top_context, type_hint, background):
    new_context = await bidi_session.browsing_context.create(type_hint=type_hint, background=background)

    try:
        # A new window opened in the background might not be fully covered by the window opened previously, so it might still be visible,
        # in that case we just make sure that a previously opened window, which should be focused now, is visible.
        if background:
            assert await get_visibility_state(bidi_session, top_context) == "visible"
            if type_hint == "tab":
                assert await get_visibility_state(bidi_session, new_context) == "hidden"
        else:
            assert await get_visibility_state(bidi_session, new_context) == "visible"

        assert await get_document_focus(bidi_session, new_context) != background
    finally:
        await bidi_session.browsing_context.close(context=new_context["context"])

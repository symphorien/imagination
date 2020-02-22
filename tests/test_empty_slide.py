#!/usr/bin/env python3
""" A regression test that imagination does not crash on empty slides """

from lib import TestSuite

with TestSuite() as t:
    a, b = map(t.text2img, "ab")
    filename = t.temp / "badfile.img"

    t.start()

    # the regression was when the empty slide was not the first one
    t.add_slide(a)
    t.assert_should_save()
    t.save_as(filename)

    t.add_empty_slide()
    # check that adding an empty slide taints the project
    t.assert_should_save()
    t.save()

    # test that slide text works
    t.choose_slide(2)
    t.set_slide_text("l ")

    # test that adding text taints the project
    t.assert_should_save()
    t.add_slide(b)
    t.save()

    # test that saving and reopening a project with an empty slide works
    t.quit()
    t.start(filename)

    video = t.export()
    assert t.ocr(t.frame_at(video, 0.5)) == "a"
    # the frame contains black text with white border on black bacground.
    # so it detects two vertical bars instead of just one.
    ocr = t.ocr(t.frame_at(video, 1.5))
    assert "l" in ocr or "1" in ocr or "I" in ocr or "|" in ocr
    assert t.ocr(t.frame_at(video, 2.5)) == "b"

    t.quit()

#!/usr/bin/env python3
""" A test that imagination selects the next slide when a slide is deleted """

from lib import TestSuite

with TestSuite() as t:
    t.start()
    # avoid c because ocr hesitates between c and C
    for f in map(t.text2img, "abhdef"):
        t.add_slide(t.temp / f)
    assert t.n_slides() == 6
    t.save_as(t.temp / "remove.img")
    t.choose_slide(4)
    t.menu("Slide", "Delete")
    assert t.n_slides() == 5
    assert t.current_slide() == 4
    # State: abhef
    t.assert_should_save()
    t.goto_next_slide()
    # this also checks if we crash when removing the last slide
    t.menu("Slide", "Delete")
    assert t.n_slides() == 4
    assert t.current_slide() == 4
    # state abhe
    t.goto_first_slide()
    # this checks if we crash when removing the first slide
    t.menu("Slide", "Delete")
    assert t.n_slides() == 3
    assert t.current_slide() == 1
    # state bhe
    t.save()
    video = t.export()
    assert t.ocr(t.frame_at(video, 0.5)) == "b"
    assert t.ocr(t.frame_at(video, 1.5)) == "h"
    assert t.ocr(t.frame_at(video, 2.5)) == "e"
    t.quit()

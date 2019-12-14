import os
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from time import sleep

from dogtail.procedural import type
from dogtail.tree import SearchError, root
from dogtail.utils import run


def add_slide(filename: Path):
    """ Add a new slide """
    imagination.child(roleName="tool bar").child(description="Import pictures").button(
        ""
    ).click()
    open_file(filename)


def _save():
    """ Click on save """
    imagination.child(roleName="tool bar").child(
        description="Save the slideshow"
    ).button("").click()


def save():
    """ Save, and assert that no save as file chooser pops up """
    _save()
    try:
        imagination.child(roleName="file chooser", retry=False)
    except SearchError:
        return
    raise (RuntimeError("Saved but a file chooser popped up"))


def save_as(filename: Path):
    """ Save, and assert that no save as file chooser pops up """
    d = filename.parent
    d.resolve()
    assert filename.suffix == ".img"
    path = str(d / filename.stem)
    _save()
    filechooser = imagination.child(roleName="file chooser")
    button = filechooser.childNamed("Save")
    type(str(path))
    button.click()


def open_file(filename: Path):
    """ When a file chooser is open, open the following path """
    filename.resolve()
    filechooser = imagination.child(roleName="file chooser")
    button = filechooser.childNamed("Open")
    button.keyCombo("<Control>L")
    sleep(0.5)
    assert not button.dead
    type(str(filename))
    button.click()
    button.click()


def text2img(text: str) -> Path:
    """ Creates an image with the text in question on it. """
    filename = temp / (str(hash(text)) + ".jpg")
    subprocess.run(["convert", "-size", "400x600", "label:" + text, str(filename)])
    filename.resolve()
    return filename


def menu(root: str, item: str):
    """ Clicks on the specified menu and submenu """
    menu = imagination.menu(root)
    menu.click()
    sleep(0.1)
    menu.menuItem(item).click()


def quit():
    """ Quits.

    Fails if there is a "did you mean to quit without saving" dialog.
    """
    menu("Slideshow", "Quit")
    sleep(0.1)
    assert imagination.dead


def open_slideshow(filename: Path):
    """ Opens the specified slideshow """
    menu("Slideshow", "Open")
    open_file(filename)


def start():
    """ start imagination, returns its root dogtail object """
    run("target/bin/imagination", timeout=4)
    return root.application("imagination")


def frame_at(video: Path, seconds: float) -> Path:
    """ Extracts one frame of the video, whose path is returned

    seconds is the time of the frame. The output format is jpg.
    """
    out = temp / (str(hash((video, seconds))) + ".jpg")
    subprocess.run(
        ["ffmpeg", "-i", str(video), "-ss", str(seconds), "-vframes", "1", str(out)]
    )
    out.resolve()
    return out


def ocr(image: Path) -> str:
    """ Returns the text on the image in argument.

    Assumes that there is only one line of text.
    """
    out = temp / str(hash(image))
    intermediate = temp / (str(hash(image)) + ".jpg")
    subprocess.run(["convert", str(image), "-fuzz", "1%", "-trim", str(intermediate)])
    subprocess.run(["tesseract", "-psm", "7", str(intermediate), str(out)])
    with open(f"{out}.txt", "r") as f:
        txt = f.read().strip()
        print(f"ocr={txt}")
        return txt


def n_slides() -> int:
    """ Returns the current number of slides """
    label = imagination.child(description="Total number of slides")
    n = int(label.name)
    print("n_slides =", n)
    return n


def export() -> Path:
    """ Export the slideshow to a file whose path is returned """
    out = temp / "export.vob"
    menu("Slideshow", "Export")
    imagination.child("Export Settings").child(roleName="text").click()
    type(str(out))
    imagination.child(roleName="dialog").button("OK").click()
    pause = imagination.child("Exporting the slideshow").child("Pause")
    while pause.showing:
        sleep(0.3)
    imagination.child("Exporting the slideshow").button("Close").click()
    out.resolve()
    return out


def choose_slide(i: int):
    """ Choose slide index """
    entry = imagination.child(description="Current slide number")
    entry.click()
    entry.keyCombo("<Control>A")
    type(str(i)+"\n")


def set_transition_type(category: str, name: str):
    """ Set the transition type of the current slide """
    combo = imagination.child("Slide Settings").child(description="Transition type")
    combo.click()
    menu = combo.menu(category)
    menu.click()
    menu.menuItem(name).click()


if __name__ == "__main__":
    with TemporaryDirectory() as d:
        temp = Path(d)
        temp.resolve()
        os.environ["LC_ALL"] = "C"

        imagination = start()
        add_slide(text2img("AB"))
        add_slide(text2img("CD"))
        choose_slide(2)
        set_transition_type("Bar Wipe", "Left to Right")
        slideshow = temp / "result.img"
        save_as(slideshow)
        assert n_slides() == 2
        quit()

        imagination = start()
        open_slideshow(slideshow)
        assert n_slides() == 2
        video = export()
        assert ocr(frame_at(video, 0.5)) == "AB"
        assert ocr(frame_at(video, 5.5)) == "CD"
        # just in the middle of the transition
        assert ocr(frame_at(video, 3)) == "CB"
        quit()

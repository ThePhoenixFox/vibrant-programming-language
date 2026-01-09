let slideIndex = 0;
let slideMax = 3;

function slideForward(item) {
    slideIndex += 1;
    slideIndex %= slideMax;
    showSlides(item, slideIndex);
}

function slideBackward(item) {
    slideIndex -= 1;
    if (slideIndex < 0) {
        slideIndex = slideMax - 1;
    }
    showSlides(item, slideIndex);
}

function showSlides(item, slideNumber) {
    const slides = item.parentElement.getElementsByClassName("slide");
    for (i = 0; i < slideMax; i++) {
        slides[i].style.display = "none";
    }
    slides[slideNumber].style.display = "block";
}
#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>

#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <limits>
#include <vector>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size): mSize(size) {};

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        if (rectSize.x > mSize.x) {
            return {false, std::numeric_limits<size_t>::max()};
        }
        if (rectSize.x+xOffset> mSize.x){
            if (rectSize.y+yOffset + mLineMaxHeight>mSize.y){
                return {false, std::numeric_limits<size_t>::max()};
            }else{
                xOffset = 0;
                yOffset += mLineMaxHeight;
                mLineMaxHeight = 0;
            }
        }else if (rectSize.y+yOffset>mSize.y){
            return {false, std::numeric_limits<size_t>::max()};
        }
        mLineMaxHeight = std::max(mLineMaxHeight, rectSize.y);
        mRects.push_back(sf::FloatRect({xOffset, yOffset}, {rectSize.x, rectSize.y}));
        xOffset += rectSize.x;
        return {true, mRects.size()-1};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const { 
        return {mRects[rectID]}; 
    }

private:
    sf::Vector2f mSize;
    float yOffset = 0;
    float xOffset = 0;
    float mLineMaxHeight = 0;
    std::vector<sf::FloatRect> mRects;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, size_t pixels) { 
    sf::Vector2u size = img.getSize();
    sf::Vector2u newSize = {img.getSize().x+static_cast<unsigned>(pixels*2), img.getSize().y+static_cast<unsigned>(pixels*2)};
    sf::Image newImage(newSize);
    for (int i = 0; i<newSize.x; i++){
        for (int j = 0; j<newSize.y; j++){
            int sourceX = std::clamp(static_cast<int>(i) - static_cast<int>(pixels), 0, static_cast<int>(size.x) - 1);
            int sourceY = std::clamp(static_cast<int>(j) - static_cast<int>(pixels), 0, static_cast<int>(size.y) - 1);
            newImage.setPixel({static_cast<unsigned>(i), static_cast<unsigned>(j)}, img.getPixel({static_cast<unsigned>(sourceX), static_cast<unsigned>(sourceY)}));
        }
    }
    return newImage;
};

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::filesystem::path& filepath) { 
        sf::Image img;
        if (!img.loadFromFile(filepath)){
            return std::numeric_limits<TextureID>::max();
        }
        sf::Image paddedImg = padImage(img, mPadding);
        auto [success, id] = mPacker.pack(sf::Vector2f(paddedImg.getSize()));
        if (!success) {
            return std::numeric_limits<TextureID>::max();
        }

        sf::FloatRect bounds = mPacker.getBounds(id);

        sf::Vector2u position(
            static_cast<unsigned>(bounds.position.x),
            static_cast<unsigned>(bounds.position.y)
        );
        mTexture.update(paddedImg, position);

        return id;
    }

    sf::FloatRect getBoundsUV(TextureID id) const { 
        sf::FloatRect bounds = mPacker.getBounds(id);
        sf::Vector2u atlasSize = mTexture.getSize();
        float x = bounds.position.x + mPadding;
        float y = bounds.position.y + mPadding;

        float width = bounds.size.x - 2 * mPadding;
        float height = bounds.size.y - 2 * mPadding;

        float u = x / static_cast<float>(atlasSize.x);
        float v = 1.0 -(y / static_cast<float>(atlasSize.y));

        float uvWidth = width / static_cast<float>(atlasSize.x);
        float uvHeight = height / static_cast<float>(atlasSize.y);

        return sf::FloatRect({u, v}, {uvWidth, uvHeight});
}

    sf::Vector2u getSizePixels(TextureID id) const { 
        sf::FloatRect bounds = mPacker.getBounds(id);
        sf::Vector2u size(static_cast<unsigned>(bounds.size.x)-mPadding*2,static_cast<unsigned>(bounds.size.y)-mPadding*2);
        return size;
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H

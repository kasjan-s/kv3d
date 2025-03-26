#pragma once

#include <memory>

#include <glm/glm.hpp>

enum class MaterialType {
    kGold,
    kEmerald,
    kPlastic
};

class Material {
public:
    virtual glm::vec3 ambient() const = 0;
    virtual glm::vec3 diffuse() const = 0;
    virtual glm::vec3 specular() const = 0;
    virtual float shininess() const = 0;
};

class GoldMaterial : public Material {
public:
    glm::vec3 ambient() const override {
        return glm::vec3(0.24725f, 0.1995f, 0.0745f);
    };

    glm::vec3 diffuse() const override {
        return glm::vec3(0.75164f, 0.60648f, 0.22648f);
    };

    glm::vec3 specular() const override {
        return glm::vec3(0.628281f, 0.555802f, 0.366065f);
    };

    float shininess() const override {
        return 0.4f * 128.0f;
    };
};

class EmeraldMaterial : public Material {
public:
    glm::vec3 ambient() const override {
        return glm::vec3(0.0215f, 0.1745f, 0.0215f);
    };

    glm::vec3 diffuse() const override {
        return glm::vec3(00.07568f, 0.61424f, 0.07568f);
    };

    glm::vec3 specular() const override {
        return glm::vec3(0.633f, 0.727811f, 0.633f);
    };

    float shininess() const override {
        return 0.6f * 128.0f;
    };
};

class PlasticMaterial : public Material {
    public:
        glm::vec3 ambient() const override {
            return glm::vec3(0.0f, 0.1f, 0.06f);
        };
    
        glm::vec3 diffuse() const override {
            return glm::vec3(0.0f, 0.50980392f, 0.50980392f);
        };
    
        glm::vec3 specular() const override {
            return glm::vec3(0.50196078f, 0.50196078f, 0.50196078f);
        };
    
        float shininess() const override {
            return 0.6f * 128.0f;
        };
};

std::unique_ptr<Material> getMaterial(MaterialType material);

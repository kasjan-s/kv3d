#include "main/material.h"

std::unique_ptr<Material> getMaterial(MaterialType material) {  
    switch (material) {
        case MaterialType::kGold:
            return std::make_unique<GoldMaterial>();
        case MaterialType::kEmerald:
            return std::make_unique<EmeraldMaterial>();
        case MaterialType::kPlastic:
            return std::make_unique<PlasticMaterial>();
    }
};
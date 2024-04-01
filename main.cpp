// #define DEBUG_CONSOLE // Uncomment this if you want a debug console to start. You can use the Console class to print. You can use Console::inStrings to get input.

#include <4dm.h>
#include <glm/gtc/random.hpp>

using namespace fdm;

// Initialize the DLLMain
initDLL

$hook(void, StateIntro, init, StateManager& s)
{
	original(self, s);

	// initialize opengl stuff
	glewExperimental = true;
	glewInit();
	glfwInit();
}

// for some reason, the game crashes if the recipes are loaded twice
bool recipesLoaded = false;
$hookStaticByFunc(void, Func::CraftingMenu::loadRecipes)
{
	if (recipesLoaded)
	{
		return;
	}
	recipesLoaded = true;
	original();

	CraftingMenu::recipes->push_back(
		nlohmann::json{
			{"recipe", {{{"name", "Stick"}, {"count", 1}}, {{"name", "Iron Bars"}, {"count", 3}}}},
			{"result", {{"name", "Iron Axe"}, {"count", 1}}}
		}
	);

	CraftingMenu::recipes->push_back(
		nlohmann::json{
			{"recipe", {{{"name", "Stick"}, {"count", 1}}, {{"name", "Deadly Bars"}, {"count", 3}}}},
			{"result", {{"name", "Deadly Axe"}, {"count", 1}}}
		}
	);
}

$hook(void, ItemTool, renderEntity, const m4::Mat5& mat, bool inHand, const glm::vec4& lightDir)
{
	std::string name = self->getName();
	if (name == "Iron Axe" || name == "Deadly Axe")
	{
		glm::vec3 colorA;
		glm::vec3 colorB;
		if (name == "Iron Axe")
		{
			colorA = glm::vec3{ 0.7f };
			colorB = colorA;
		}
		else
		{
			colorA = glm::vec3{ 0.7, 0, 0.7 };
			colorB = glm::vec3{ 0, 0.7, 0.7 };
		}

		// don't question this, it works
		m4::Mat5 axeMat{ 1 };

		axeMat.translate(glm::vec4{ 0, 1.3, 0.2f, 0 });

		axeMat *= m4::Mat5(m4::Rotor{ m4::wedge({0, 0, 1, 0}, {0, 1, 0, 0}), glm::pi<float>() / 4 });

		axeMat.scale(glm::vec4{ 0.1f, 1.0f, 1.0f, 1.0f });

		axeMat.translate(glm::vec4{ -0.5f, -0.5f, -0.5f, -0.5f });

		m4::Mat5 axeMat2{ 1 };
		axeMat2.translate(glm::vec4{ 0, 0, 1.5f, 0 });
		axeMat2 = axeMat * axeMat2;

		axeMat *= mat;
		axeMat2 *= mat;

		const Shader* axeShader = ShaderManager::get("tetSolidColorNormalShader");
		axeShader->use();
		glUniform4fv(glGetUniformLocation(axeShader->id(), "lightDir"), 1, &lightDir[0]);

		glUniform4f(glGetUniformLocation(axeShader->id(), "inColor"), colorA.r, colorA.g, colorA.b, 1);
		glUniform1fv(glGetUniformLocation(axeShader->id(), "MV"), sizeof(axeMat) / sizeof(float), &axeMat[0][0]);

		ItemMaterial::barRenderer->render();

		glUniform4f(glGetUniformLocation(axeShader->id(), "inColor"), colorB.r, colorB.g, colorB.b, 1);
		glUniform1fv(glGetUniformLocation(axeShader->id(), "MV"), sizeof(axeMat2) / sizeof(float), &axeMat2[0][0]);

		ItemMaterial::barRenderer->render();
	}
	original(self, mat, inHand, lightDir);
}

$hook(bool, ItemTool, action, World* world, Player* player, int action)
{
	if (action == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (player->targetingBlock)
		{
			// get the block type
			uint8_t block = world->getBlock(player->targetBlock);

			if (block == BlockInfo::LAVA)
			{
				return false;
			}

			if (self->name == "Iron Axe")
			{
				if (block == BlockInfo::WOOD || block == BlockInfo::MIDNIGHT_WOOD)
				{
					player->targetDamage += 0.1;
				}
				else
				{
					player->targetDamage += 0.005;
				}
				return false;
			}
			
			if (self->name == "Deadly Axe")
			{
				if (block == BlockInfo::WOOD || block == BlockInfo::MIDNIGHT_WOOD)
				{
					player->targetDamage += 0.2;
				}
				else
				{
					player->targetDamage += 0.01;
				}
				return false;
			}
		}
	}
	return original(self, world, player, action);
}

bool normalAxeBreak(World* world, Player* player, uint8_t block, const glm::ivec4& blockPos)
{
	if (block != BlockInfo::DEADLY_ORE)
	{
		if (!world->setBlockUpdate(blockPos, BlockInfo::AIR))
		{
			return false;
		}

		glm::vec3 vel3 = glm::ballRand(0.03f);

		glm::vec4 targetBlock = player->targetBlock;

		Chunk* c = world->getChunkFromCoords(targetBlock.x, targetBlock.z, targetBlock.w);
		if (c != nullptr)
		{
			// create the item entity
			nlohmann::json j{
				{"name", BlockInfo::blockNames->at(block)},
				{"count", 1}
			};
			std::unique_ptr<Entity> ie = EntityItem::createWithItem(
				Item::createFromJson(j),
				targetBlock + glm::vec4{ 0.5, 0.5, 0.5, 0.5 },
				{ vel3.x, 0.05, vel3.y, vel3.z }
			);
			world->addEntityToChunk(ie, c);
			return true;
		}
	}
	return false;
}

$hook(bool, ItemTool, breakBlock, World* world, Player* player, uint8_t block, const glm::ivec4& blockPos)
{
	if (self->name == "Iron Axe")
	{
		return normalAxeBreak(world, player, block, blockPos);
	}

	if (self->name == "Deadly Axe")
	{
		if (block == BlockInfo::WOOD || block == BlockInfo::MIDNIGHT_WOOD)
		{
			for (auto& entry : world->chunks)
			{
				Chunk* chunk = entry.second.get();
				if (!chunk->loaded)
				{
					continue;
				}

				glm::ivec4 chunkOffset = glm::ivec4{ chunk->pos[0], 0, chunk->pos[1], chunk->pos[2] } * 8;

				constexpr uint32_t endPos = 8 * 8 * 8 * 128;
				for (uint32_t blockPos = 0; blockPos < endPos; ++blockPos)
				{
					int x = (blockPos >> 13) & 7;
					int y = (blockPos >> 6) & 127;
					int z = (blockPos >> 3) & 7;
					int w = (blockPos >> 0) & 7;

					if (chunk->blocks[x + 1][z + 1][w + 1][y] != block)
					{
						continue;
					}

					if (!world->setBlockUpdate(glm::ivec4{ x, y, z, w } + chunkOffset, BlockInfo::AIR))
					{
						continue;
					}

					glm::vec3 vel3 = glm::ballRand(0.03f);

					glm::vec4 targetBlock = player->targetBlock;

					Chunk* c = world->getChunkFromCoords(targetBlock.x, targetBlock.z, targetBlock.w);
					if (c != nullptr)
					{
						// create the item entity
						nlohmann::json j{
							{"name", BlockInfo::blockNames->at(block)},
							{"count", 1}
						};
						std::unique_ptr<Entity> ie = EntityItem::createWithItem(
							Item::createFromJson(j),
							targetBlock + glm::vec4{ 0.5, 0.5, 0.5, 0.5 },
							{ vel3.x, 0.05, vel3.y, vel3.z }
						);
						world->addEntityToChunk(ie, c);
					}
				}
			}
			return true;
		}
		else if (block != BlockInfo::DEADLY_ORE)
		{
			return normalAxeBreak(world, player, block, blockPos);
		}
	}
	return original(self, world, player, block, blockPos);
}

$hook(bool, ItemTool, entityAction, World* world, Player* player, Entity* entity, int action)
{
	if (action == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (self->name == "Iron Axe")
		{
			entity->takeDamage(5, world);
			return true;
		}

		if (self->name == "Deadly Axe")
		{
			entity->takeDamage(50, world);
			return true;
		}
	}
	return original(self, world, player, entity, action);
}

$hookStaticByFunc(void, Func::Item::renderItemDescription, std::unique_ptr<Item>& item, const glm::ivec2& pos)
{
	int renderX = pos.x + 5;
	int renderY = pos.y - 25;

	int charWidth = 16;
	int charHeight = 16;
	
	std::string text = item->getName();
	std::string_view target = "Midnight";
	size_t index = text.find(target, 0);
	if (index != std::string::npos)
	{
		text.replace(index, target.length(), "Morning");
	}

	std::cout << text << std::endl;

	// render background quad
	Item::qr->setPos(renderX - 5, renderY - 5, charWidth * text.length() + 10, charHeight + 10);
	Item::qr->setQuadRendererMode(GL_TRIANGLES);
	Item::qr->setColor(0, 0, 0, 1);
	Item::qr->render();
	Item::qr->setQuadRendererMode(GL_LINE_LOOP);

	// text
	Item::fr->setText(text);
	Item::fr->fontSize = 2;
	if (item->isDeadly())
	{
		glm::ivec2 offset1 = glm::diskRand(5.0f);
		glm::ivec2 offset2 = glm::diskRand(5.0f);

		Item::qr->setPos(renderX - 5, renderY - 5, charWidth * text.length() + 10, charHeight + 10);
		glm::vec4 colors[] = {
			{1, 1, 1, 1},
			{0, 1, 1, 1},
			{0, 1, 1, 1},
			{1, 0, 1, 1},
		};
		glBindVertexArray(Item::qr->VAO);

		glBindBuffer(GL_ARRAY_BUFFER, Item::qr->buffers[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, colors, GL_STATIC_DRAW);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		Item::qr->render();

		Item::fr->color = { 1, 0, 1, 0.7 };
		Item::fr->pos = { renderX + offset1.x, renderY + offset1.y };
		Item::fr->updateModel();
		Item::fr->render();

		Item::fr->color = { 0, 1, 1, 0.7 };
		Item::fr->pos = { renderX + offset2.x, renderY + offset2.y };
		Item::fr->updateModel();
		Item::fr->render();
	}
	else
	{
		Item::qr->setColor(0.5, 0, 0.5, 1.0);
		Item::qr->render();
	}
	Item::fr->color = { 1, 1, 1, 1 };
	Item::fr->pos = { renderX, renderY };
	Item::fr->updateModel();
	Item::fr->render();
}
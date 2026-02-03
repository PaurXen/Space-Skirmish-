# Scenario Configuration Files

This directory contains battle scenario configuration files for Space Skirmish.

## Usage

Run Command Center with a specific scenario:

```bash
./command_center --scenario asteroid_field
./command_center --scenario siege
./command_center --scenario fleet_battle
```

If no scenario is specified, the default scenario is used.

## Available Scenarios

### default.conf
Classic 4 carriers at corners battle. Standard balanced engagement.
- 2 Republic carriers
- 2 CIS carriers
- No obstacles

### asteroid_field.conf
Navigate through a dangerous asteroid belt while engaging the enemy.
- Mixed forces with fighters
- Central asteroid belt creates strategic chokepoints
- Random placement for varied gameplay

### siege.conf
Republic defends a fortified position against CIS assault.
- Manual unit placement for precise tactical setup
- Republic has defensive walls/obstacles
- CIS has superior numbers for assault
- Tests defensive vs offensive strategies

### fleet_battle.conf
Large-scale engagement with capital ships and fighter support.
- Multiple carriers and destroyers per side
- 6 fighters per faction
- Line formation for epic battle lines
- Tests fleet coordination

### skirmish.conf
Fast-paced fighter-only engagement.
- 8 fighters per side
- No capital ships
- Scattered placement
- Quick, action-packed battles

## Configuration Format

### Sections

#### [scenario]
- `name` - Display name for the scenario

#### [map]
- `width` - Map width (40-200, default 80)
- `height` - Map height (20-100, default 40)

#### [obstacles]
- `add=x,y` - Add obstacle at coordinates (can have multiple)

#### [republic] / [cis]
- `carriers` - Number of carriers
- `destroyers` - Number of destroyers
- `fighters` - Number of fighters/squadrons
- `placement` - Placement mode (corners, edges, random, line, scattered)

#### [units]
Manual unit placement (overrides auto-generation):
- `add=type,faction,x,y` - Add specific unit
  - Types: carrier, destroyer, flagship, fighter, bomber, elite (or 1-6)
  - Factions: republic, cis (or 1, 2)

## Example Custom Scenario

```ini
[scenario]
name=My Custom Battle

[map]
width=100
height=50

[obstacles]
add=50,25
add=51,25
add=52,25

[republic]
carriers=1
destroyers=2
fighters=4
placement=random

[cis]
carriers=2
destroyers=1
fighters=3
placement=line
```

## Creating Your Own Scenarios

1. Copy an existing .conf file
2. Modify parameters to your liking
3. Save in `scenarios/` directory
4. Run with `./command_center --scenario your_scenario_name`

### Tips

- **Obstacles** create strategic terrain - use them for defensive positions or to create chokepoints
- **Placement modes**:
  - `corners` - Traditional opposite corners
  - `random` - Random positions in respective halves
  - `line` - Battle line formation
  - `scattered` - Random with spacing
- **Manual placement** gives precise control but requires careful positioning
- Test with `--scenario` to iterate quickly

## Obstacle Patterns

Obstacles appear as gray `#` symbols on the grid. Use them to create:
- **Walls** - Linear barriers for defense
- **Asteroid fields** - Scattered debris for navigation challenges  
- **Chokepoints** - Narrow passages forcing tactical decisions
- **Safe zones** - Protected areas behind obstacles

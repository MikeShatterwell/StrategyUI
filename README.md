# StrategyUI

StrategyUI is a flexible, extensible, open-source UI plugin designed to be used for creating widgets with procedural layouts.

REQUIRES https://github.com/MikeShatterwell/AsyncWidgetLoader

Examples of such widgets include:
- Radial menus
- Infinitely scrolling spiral item lists

Things that should be possible with StrategyUI
- Floating world space UI elements (e.g, objective markers, compass points, interaction indicators, etc.)
- Interactive maps with zoom and pan functionality 
- Minimap displays
- Tooltips and context menus 
  - Bonus points for timed sticky tooltips that will freeze and allow interaction on the new tooltip
 

This whole system is a bit unhinged, to be honest. Have fun poking around!


## License

StrategyUI is licensed under the MIT License.
Do what you want with it 😄

---

## Table of Contents

- [Overview](#overview)
- [Key Components](#key-components)
  - [Layout Strategies](#layout-strategies)
  - [Base Strategy Widget](#base-strategy-widget)
  - [Data Providers](#data-providers)
  - [Entry Widgets & Interfaces](#entry-widgets--interfaces)
  - [Utilities & Settings](#utilities--settings)
- [Integration](#integration)
- [Extending StrategyUI](#extending-strategyui)
  - [Creating a Custom Layout Strategy](#creating-a-custom-layout-strategy)
  - [Customizing Data Providers](#customizing-data-providers)
  - [Developing Custom Entry Widgets](#developing-custom-entry-widgets)
- [Design Patterns at a Glance](#design-patterns-at-a-glance)
- [Contributing & Further Customization](#contributing--further-customization)
- [License](#license)
- [Acknowledgements](#acknowledgements)

---

## Overview

StrategyUI is intended to separate data & widget management from the layout algorithms in UI layout.
 
The strategy maps an item index to layout data relevant for its bound entry widget.
This separation allows you to easily swap out or extend layout algorithms without altering the core widget logic.

---

## Key Components

### Overview (terminology)
- **Global Index:** An unbound, unique index that a LayoutStrategy can use to compute layout data or convert to a Data Index.
  - Usually, a GlobalIndex can map directly to a Data Index -- never going negative or exceeding the data array size.
  - However, for infinite scrolling or other advanced layouts, the GlobalIndex _may be_ negative or exceed the data array size
    - Override `int32 GlobalIndexToDataIndex(int32 GlobalIndex)` in your LayoutStrategy to handle these cases.


- **Data Index:** The index of the data item in the data provider’s array.
  - Should always be a valid index within the data array.


- **Entry Widget:** A single widget that implements `IStrategyEntryBase` to represent a single data item in the UI.


- **Data Provider:** A class that implements `IStrategyDataProvider` to supply data items to the widget container.


- **Layout Strategy:** A class that extends `UBaseLayoutStrategy` to compute layout data for a given Global Index.


- **Base Strategy Widget:** A widget that inherits from `UBaseStrategyWidget` and integrates layout strategies, data providers, and entry widgets.



### Layout Strategies

- **UBaseLayoutStrategy**  
  An abstract base class that defines the interface for layout calculations such as:
  - `FVector2D GetItemPosition(int32 GlobalIndex)`
  - `TSet<int32> ComputeDesiredGlobalIndices()`
  - `int32 GlobalIndexToDataIndex(int32 GlobalIndex)`

- **Concrete Strategies**  
  - **URadialLayoutStrategy:** Positions items on a circle using a base radius and evenly spaced segments.
  - **USpiralLayoutStrategy:** Extends radial logic to create an infinite spiral layout with variable radii.
  - **UWheelLayoutStrategy:** Arranges items uniformly around a full 360° wheel.

These strategies encapsulate layout logic so that the widget container only needs to ask, "Where does item X go?"

---

### Base Strategy Widget

- **UBaseStrategyWidget**  
  The central container widget that integrates:
  - **Layout Strategy:** Delegates layout computations.
  - **Data Management:** Works with data providers to fetch and update data items.
  - **Widget Pooling:** Reuses entry widgets via `FUserWidgetPool` for efficiency.
  - **State Management:** Uses gameplay tags to manage focus and selection states.
  - **Event Broadcasting:** Notifies about data updates, focus changes, and selection events.

---

### Data Providers

- **IStrategyDataProvider Interface**  
  Defines how a data provider supplies an array of data items (UObjects) to the widget.

- **UDebugItemsDataProvider**  
  A sample implementation that generates a configurable number of debug items for testing purposes.

---

### Entry Widgets & Interfaces

- **IStrategyEntryBase**  
  An interface that entry widgets must implement. It declares events for:
  - State changes
  - Item assignment
  - Selection and focus updates

- **IStrategyEntryWidgetProvider**  
  Optionally implemented by data items to specify a custom entry widget class or a widget tag.

- **IRadialItemEntry**  
  A specialized interface for entry widgets used in radial layouts, including support for dynamic material data.

---

### Utilities & Settings

- **StrategyUIProjectSettings**  
  A Developer Settings class where you can map gameplay tags to widget classes, configure warning behavior, and adjust global plugin settings.

- **StrategyUIFunctionLibrary**  
  A BlueprintFunctionLibrary with utility functions (e.g., for widget class lookups based on gameplay tags).

- **Logging & Debug Utilities**  
  Logging is provided via the `LogStrategyUI` category, and utilities like `FLayoutStrategyDebugPaintUtil` allow you to visually debug your layout geometry.

---

## Integration

To integrate StrategyUI into your project:

1. **Install the Plugin**  
   Place the StrategyUI plugin folder into your project’s `Plugins` directory and enable it via the Plugin Manager.

2. **Setup Your Widget**  
   - Create a UI widget (using UMG) that inherits from `UBaseStrategyWidget`.
   - Assign a layout strategy (e.g., `URadialLayoutStrategy`, `USpiralLayoutStrategy`, or `UWheelLayoutStrategy`) to the widget’s `LayoutStrategy` property.
   - Set a default entry widget class that implements `IStrategyEntryBase`.

3. **Configure a Data Provider**  
   - Use a data provider (e.g., `UDebugItemsDataProvider`) that implements `IStrategyDataProvider` or bind your widget to a view model if using MVVM.
   - The widget will automatically fetch data and update when the provider broadcasts changes.

4. **Bind Events & Customize Behavior**  
   - Hook into delegate events such as `OnItemFocused`, `OnItemSelected`, and `OnPointerRotationUpdated` to implement custom responses.
   - Use debug drawing options in editor builds to visualize layout parameters during development.

---

## Extending StrategyUI

### Creating a Custom Layout Strategy

1. **Subclass UBaseLayoutStrategy**  
   Create your own layout strategy by deriving from `UBaseLayoutStrategy` (or an existing derivative if you wish to extend a radial algorithm).

2. **Override Virtual Functions**  
   Implement key methods such as:
   - `GetItemPosition(int32 GlobalIndex)`
   - `ComputeDesiredGlobalIndices()`
   - `ComputeEntryWidgetSize(int32 GlobalIndex)`
   - Any additional helper methods for managing angles, gaps, or distance factors.

3. **Integrate Your Strategy**  
   Assign an instance of your custom strategy to the `LayoutStrategy` property of your widget. Your new strategy will be used automatically for layout computations.

---

### Customizing Data Providers

- **Implement IStrategyDataProvider**  
  Develop a new data provider class that implements the `IStrategyDataProvider` interface to supply your own data set.

- **Broadcast Data Updates**  
  Use the provided delegate wrapper (`UOnDataProviderUpdatedDelegateWrapper`) to notify the widget when data changes occur.

---

### Developing Custom Entry Widgets

1. **Implement IStrategyEntryBase**  
   Create a new entry widget (derived from `UUserWidget`) that implements `IStrategyEntryBase` to handle state changes, data assignment, and interactions.

2. **Optionally Implement IStrategyEntryWidgetProvider**  
   If your data items determine their own entry widget type, implement this interface in your data objects to specify the widget class or tag.

3. **Add Dynamic Material Effects (Optional)**  
   For radial layouts that use dynamic materials, implement `IRadialItemEntry` to receive material data for visual effects.

---

## Contributing & Further Customization

- **Extending Functionality:**  
  You are encouraged to add new layout strategies, data providers, or custom entry widget behaviors by following the interfaces and base classes provided.

- **Debugging & Logging:**  
  Utilize the integrated logging (`LogStrategyUI`) and debug drawing utilities (via `FLayoutStrategyDebugPaintUtil`) to help diagnose and improve your customizations.

- **Community & Issues:**  
  Contributions, bug reports, and feature requests are welcome.

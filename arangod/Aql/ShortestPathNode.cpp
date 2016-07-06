/// @brief Implementation of Shortest Path Execution Node
///
/// @file arangod/Aql/ShortestPathNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ShortestPathNode.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Indexes/Index.h"
#include "V8Server/V8Traverser.h"
#include "Utils/CollectionNameResolver.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

static void parseNodeInput(AstNode const* node, std::string& id,
                           Variable const*& variable) {
  switch (node->type) {
    case NODE_TYPE_REFERENCE:
      variable = static_cast<Variable*>(node->getData());
      id = "";
      break;
    case NODE_TYPE_VALUE:
      if (node->value.type != VALUE_TYPE_STRING) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                       "invalid start vertex. Must either be "
                                       "an _id string or an object with _id.");
      }
      variable = nullptr;
      id = node->getString();
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     "invalid start vertex. Must either be an "
                                     "_id string or an object with _id.");
  }
}

static TRI_edge_direction_e parseDirection (uint64_t const dirNum) {
  switch (dirNum) {
    case 0:
      return TRI_EDGE_ANY;
    case 1:
      return TRI_EDGE_IN;
    case 2:
      return TRI_EDGE_OUT;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          "direction can only be INBOUND, OUTBOUND or ANY");
  }
}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan, size_t id,
                                   TRI_vocbase_t* vocbase, uint64_t direction,
                                   AstNode const* start, AstNode const* target,
                                   AstNode const* graph,
                                   ShortestPathOptions const& options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr),
      _graphObj(nullptr),
      _options(options) {

  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(start != nullptr);
  TRI_ASSERT(target != nullptr);
  TRI_ASSERT(graph != nullptr);

  TRI_edge_direction_e baseDirection = parseDirection(direction);

  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    size_t edgeCollectionCount = graph->numMembers();
    auto resolver = std::make_unique<CollectionNameResolver>(vocbase);
    _graphJson = arangodb::basics::Json(arangodb::basics::Json::Array,
                                        edgeCollectionCount);
    _edgeColls.reserve(edgeCollectionCount);
    _directions.reserve(edgeCollectionCount);
    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      auto col = graph->getMember(i);
      if (col->type == NODE_TYPE_DIRECTION) {
        // We have a collection with special direction.
        TRI_ASSERT(col->getMember(0)->isIntValue());
        TRI_edge_direction_e dir = parseDirection(col->getMember(0)->getIntValue());
        _directions.emplace_back(dir);
        col = col->getMember(1);
      } else {
        _directions.emplace_back(baseDirection);
      }

      std::string eColName = col->getString();
      auto eColType = resolver->getCollectionTypeCluster(eColName);
      if (eColType != TRI_COL_TYPE_EDGE) {
        std::string msg("collection type invalid for collection '" +
                        std::string(eColName) +
                        ": expecting collection type 'edge'");
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
                                       msg);
      }
      _graphJson.add(arangodb::basics::Json(eColName));
      _edgeColls.push_back(eColName);
    }
  } else {
    if (_edgeColls.empty()) {
      if (graph->isStringValue()) {
        std::string graphName = graph->getString();
        _graphJson = arangodb::basics::Json(graphName);
        _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

        if (_graphObj == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
        }

        auto eColls = _graphObj->edgeCollections();
        size_t length = eColls.size();
        if (length == 0) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
        }
        _edgeColls.reserve(length);
        _directions.reserve(length);

        for (const auto& n : eColls) {
          _edgeColls.push_back(n);
          _directions.emplace_back(baseDirection);
        }
      }
    }
  }

  parseNodeInput(start, _startVertexId, _inStartVariable);
  parseNodeInput(target, _targetVertexId, _inTargetVariable);
}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan, size_t id,
                                   TRI_vocbase_t* vocbase,
                                   std::vector<std::string> const& edgeColls,
                                   std::vector<TRI_edge_direction_e> directions,
                                   Variable const* inStartVariable,
                                   std::string const& startVertexId,
                                   Variable const* inTargetVariable,
                                   std::string const& targetVertexId,
                                   ShortestPathOptions const& options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _inStartVariable(inStartVariable),
      _startVertexId(startVertexId),
      _inTargetVariable(inTargetVariable),
      _targetVertexId(targetVertexId),
      _directions(directions),
      _graphObj(nullptr),
      _options(options) {
  _graphJson = arangodb::basics::Json(arangodb::basics::Json::Array, edgeColls.size());
  for (auto& it : edgeColls) {
    _edgeColls.emplace_back(it);
    _graphJson.add(arangodb::basics::Json(it));
  }
}

void ShortestPathNode::fillOptions(arangodb::traverser::ShortestPathOptions& opts) const {
  if (!_options.weightAttribute.empty()) {
    opts.useWeight = true;
    opts.weightAttribute = _options.weightAttribute;
    opts.defaultWeight = _options.defaultWeight;
  } else {
    opts.useWeight = false;
  }
}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan,
                                   arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr),
      _graphObj(nullptr) {
  // Directions
  auto dirList = base.get("directions");
  TRI_ASSERT(dirList.json() != nullptr);
  for (size_t i = 0; i < dirList.size(); ++i) {
    auto dirJson = dirList.at(i);
    uint64_t dir = arangodb::basics::JsonHelper::stringUInt64(dirJson.json());
    TRI_edge_direction_e d;
    switch (dir) {
      case 0:
        d = TRI_EDGE_ANY;
        break;
      case 1:
        d = TRI_EDGE_IN;
        break;
      case 2:
        d = TRI_EDGE_OUT;
        break;
      default:
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "Invalid direction value");
        break;
    }
    _directions.emplace_back(d);
  }

  // Start Vertex
  if (base.has("startInVariable")) {
    _inStartVariable = varFromJson(plan->getAst(), base, "startInVariable");
  } else {
    _startVertexId = arangodb::basics::JsonHelper::getStringValue(
        base.json(), "startVertexId", "");
    if (_startVertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex mustn't be empty.");
    }
  }

  // Target Vertex
  if (base.has("targetInVariable")) {
    _inTargetVariable = varFromJson(plan->getAst(), base, "targetInVariable");
  } else {
    _targetVertexId = arangodb::basics::JsonHelper::getStringValue(
        base.json(), "targetVertexId", "");
    if (_targetVertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "target vertex mustn't be empty.");
    }
  }

  std::string graphName;
  if (base.has("graph") && (base.get("graph").isString())) {
    graphName = JsonHelper::checkAndGetStringValue(base.json(), "graph");
    if (base.has("graphDefinition")) {
      _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

      if (_graphObj == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
      }

      auto eColls = _graphObj->edgeCollections();
      for (auto const& n : eColls) {
        _edgeColls.push_back(n);
      }
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "missing graphDefinition.");
    }
  } else {
    _graphJson = base.get("graph").copy();
    if (!_graphJson.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "graph has to be an array.");
    }
    size_t edgeCollectionCount = _graphJson.size();
    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      auto at = _graphJson.at(i);
      if (!at.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                       "graph has to be an array of strings.");
      }
      _edgeColls.push_back(at.json()->_value._string.data);
    }
    if (_edgeColls.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_BAD_JSON_PLAN,
          "graph has to be a non empty array of strings.");
    }
  }

  // Out variables
  if (base.has("vertexOutVariable")) {
    _vertexOutVariable = varFromJson(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.has("edgeOutVariable")) {
    _edgeOutVariable = varFromJson(plan->getAst(), base, "edgeOutVariable");
  }

  // Flags
  if (base.has("shortestPathFlags")) {
    _options = ShortestPathOptions(base);
  }
}

void ShortestPathNode::toVelocyPackHelper(VPackBuilder& nodes,
                                          bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method
  nodes.add("database", VPackValue(_vocbase->_name));
  {
    // TODO Remove _graphJson
    auto tmp = arangodb::basics::JsonHelper::toVelocyPack(_graphJson.json());
    nodes.add("graph", tmp->slice());
  }
  nodes.add(VPackValue("directions"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& d : _directions) {
      nodes.add(VPackValue(d));
    }
  }

  // In variables
  if (usesStartInVariable()) {
    nodes.add(VPackValue("startInVariable"));
    startInVariable()->toVelocyPack(nodes);
  } else {
    nodes.add("startVertexId", VPackValue(_startVertexId));
  }

  if (usesTargetInVariable()) {
    nodes.add(VPackValue("targetInVariable"));
    targetInVariable()->toVelocyPack(nodes);
  } else {
    nodes.add("targetVertexId", VPackValue(_targetVertexId));
  }

  if (_graphObj != nullptr) {
    nodes.add(VPackValue("graphDefinition"));
    _graphObj->toVelocyPack(nodes, verbose);
  }

  // Out variables
  if (usesVertexOutVariable()) {
    nodes.add(VPackValue("vertexOutVariable"));
    vertexOutVariable()->toVelocyPack(nodes);
  }
  if (usesEdgeOutVariable()) {
    nodes.add(VPackValue("edgeOutVariable"));
    edgeOutVariable()->toVelocyPack(nodes);
  }

  nodes.add(VPackValue("shortestPathFlags"));
  _options.toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

ExecutionNode* ShortestPathNode::clone(ExecutionPlan* plan,
                                       bool withDependencies,
                                       bool withProperties) const {
  auto c = new ShortestPathNode(plan, _id, _vocbase, _edgeColls, _directions,
                                _inStartVariable, _startVertexId,
                                _inTargetVariable, _targetVertexId, _options);
  if (usesVertexOutVariable()) {
    auto vertexOutVariable = _vertexOutVariable;
    if (withProperties) {
      vertexOutVariable =
          plan->getAst()->variables()->createVariable(vertexOutVariable);
    }
    TRI_ASSERT(vertexOutVariable != nullptr);
    c->setVertexOutput(vertexOutVariable);
  }

  if (usesEdgeOutVariable()) {
    auto edgeOutVariable = _edgeOutVariable;
    if (withProperties) {
      edgeOutVariable =
          plan->getAst()->variables()->createVariable(edgeOutVariable);
    }
    TRI_ASSERT(edgeOutVariable != nullptr);
    c->setEdgeOutput(edgeOutVariable);
  }

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

double ShortestPathNode::estimateCost(size_t& nrItems) const {
  // Standard estimation for Shortest path is O(|E| + |V|*LOG(|V|))
  // At this point we know |E| but do not know |V|.
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  auto trx = _plan->getAst()->query()->trx();
  auto collections = _plan->getAst()->query()->collections();
  size_t edgesCount = 0;
  double nodesEstimate = 0;

  TRI_ASSERT(collections != nullptr);

  for (auto const& it : _edgeColls) {
    auto collection = collections->get(it);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unexpected pointer for collection");
    }
    size_t edges = collection->count();

    auto indexes = trx->indexesForCollection(collection->name);
    for (auto const& index : indexes) {
      if (index->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        // We can only use Edge Index
        if (index->hasSelectivityEstimate()) {
          nodesEstimate += edges * index->selectivityEstimate();
        } else {
          // Hard-coded fallback should not happen
          nodesEstimate += edges * 0.01;
        }
        break;
      }
    }

    edgesCount += edges;
  }
  nrItems = edgesCount + static_cast<size_t>(log(nodesEstimate) * nodesEstimate);
  return depCost + nrItems;
}

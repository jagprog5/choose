#pragma once

#include "pipeline/unit/unit.hpp"

// namespace pipeline {

// struct Head : public PipelineUnit {
//   size_t n;

//   Head(NextUnit next, size_t n) : PipelineUnit(std::move(next)), n(n) {
//     if (this->n == 0) {
//       this->process(EndOfStream());
//     }
//   }
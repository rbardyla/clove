"""
HRM+DNC Web Data Integration Layer
Connects AI Web Extraction Engine with Hierarchical Reinforcement Memory
and Differentiable Neural Computer Architecture
"""

import asyncio
import hashlib
import json
import time
from collections import deque
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.distributions import Categorical


# ==================== HRM Core Components ====================

class HighLevelPlanningAgent(nn.Module):
    """
    High-level planning agent (f_H) - The "prefrontal cortex"
    Maintains abstract state and generates high-level policies
    """
    
    def __init__(self, state_dim: int = 512, goal_dim: int = 256, 
                 memory_dim: int = 256, hidden_dim: int = 1024):
        super().__init__()
        
        # LSTM for maintaining temporal context
        self.lstm = nn.LSTM(
            state_dim + memory_dim, 
            hidden_dim, 
            num_layers=3,
            batch_first=True,
            dropout=0.2
        )
        
        # Transformer for abstract reasoning
        self.transformer = nn.TransformerEncoder(
            nn.TransformerEncoderLayer(
                d_model=hidden_dim,
                nhead=8,
                dim_feedforward=2048,
                dropout=0.1
            ),
            num_layers=6
        )
        
        # Goal generation network
        self.goal_generator = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim // 2),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(hidden_dim // 2, goal_dim),
            nn.Tanh()
        )
        
        # Policy update gate (determines when to update)
        self.update_gate = nn.Sequential(
            nn.Linear(hidden_dim, 128),
            nn.ReLU(),
            nn.Linear(128, 1),
            nn.Sigmoid()
        )
        
        # Web extraction strategy selector
        self.strategy_selector = nn.Sequential(
            nn.Linear(hidden_dim, 256),
            nn.ReLU(),
            nn.Linear(256, 4),  # 4 extraction strategies
            nn.Softmax(dim=-1)
        )
        
        self.hidden_state = None
        self.cell_state = None
        
    def forward(self, low_level_state: torch.Tensor, 
                memory_summary: torch.Tensor,
                external_state: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor, bool]:
        """
        Generate high-level goal and determine if update needed
        
        Returns:
            - goal_vector: High-level policy/goal
            - strategy_probs: Extraction strategy probabilities
            - should_update: Whether policy needs updating
        """
        # Combine inputs
        combined_input = torch.cat([low_level_state, memory_summary], dim=-1)
        combined_input = combined_input.unsqueeze(1)  # Add sequence dimension
        
        # Process through LSTM
        if self.hidden_state is None:
            lstm_out, (self.hidden_state, self.cell_state) = self.lstm(combined_input)
        else:
            lstm_out, (self.hidden_state, self.cell_state) = self.lstm(
                combined_input, (self.hidden_state, self.cell_state)
            )
            
        # Abstract reasoning with transformer
        abstract_state = self.transformer(lstm_out)
        abstract_state = abstract_state.squeeze(1)
        
        # Generate goal vector
        goal_vector = self.goal_generator(abstract_state)
        
        # Determine if update needed
        update_signal = self.update_gate(abstract_state)
        should_update = update_signal.item() > 0.5
        
        # Select extraction strategy
        strategy_probs = self.strategy_selector(abstract_state)
        
        return goal_vector, strategy_probs, should_update


class LowLevelExecutionAgent(nn.Module):
    """
    Low-level execution agent (f_L) - Translates high-level policies to actions
    Interfaces directly with DNC memory
    """
    
    def __init__(self, goal_dim: int = 256, observation_dim: int = 512,
                 memory_dim: int = 256, action_dim: int = 128):
        super().__init__()
        
        # Fast LSTM for reactive control
        self.lstm = nn.LSTM(
            goal_dim + observation_dim,
            512,
            num_layers=2,
            batch_first=True,
            dropout=0.1
        )
        
        # DNC interface networks
        self.write_vector_net = nn.Sequential(
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Linear(256, memory_dim)
        )
        
        self.read_key_net = nn.Sequential(
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Linear(256, memory_dim),
            nn.Tanh()
        )
        
        self.write_key_net = nn.Sequential(
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Linear(256, memory_dim),
            nn.Tanh()
        )
        
        # Extraction action network
        self.action_net = nn.Sequential(
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Linear(256, action_dim)
        )
        
    def forward(self, goal: torch.Tensor, 
                observation: torch.Tensor) -> Dict[str, torch.Tensor]:
        """
        Generate DNC memory operations and actions
        """
        # Combine goal and observation
        combined = torch.cat([goal, observation], dim=-1).unsqueeze(1)
        
        # Process through LSTM
        lstm_out, _ = self.lstm(combined)
        lstm_out = lstm_out.squeeze(1)
        
        # Generate DNC interface vectors
        write_vector = self.write_vector_net(lstm_out)
        read_key = self.read_key_net(lstm_out)
        write_key = self.write_key_net(lstm_out)
        
        # Generate action
        action = self.action_net(lstm_out)
        
        return {
            'write_vector': write_vector,
            'read_key': read_key,
            'write_key': write_key,
            'action': action
        }


# ==================== Enhanced DNC Memory Controller ====================

class EnhancedDNCController:
    """
    Enhanced DNC Memory Controller with web extraction capabilities
    """
    
    def __init__(self, memory_size: int = 10000, memory_dim: int = 256,
                 num_read_heads: int = 4, num_write_heads: int = 1):
        self.memory_size = memory_size
        self.memory_dim = memory_dim
        self.num_read_heads = num_read_heads
        self.num_write_heads = num_write_heads
        
        # Core memory components
        self.memory = torch.zeros(memory_size, memory_dim)
        self.usage = torch.zeros(memory_size)
        self.precedence = torch.zeros(memory_size)
        self.temporal_links = torch.zeros(memory_size, memory_size)
        
        # Web extraction specific memory
        self.url_index: Dict[str, int] = {}  # URL to memory location mapping
        self.extraction_patterns: Dict[str, torch.Tensor] = {}  # Domain patterns
        self.confidence_scores = torch.zeros(memory_size)
        
        # Read/Write heads
        self.read_weights = torch.zeros(num_read_heads, memory_size)
        self.write_weights = torch.zeros(num_write_heads, memory_size)
        
        # Statistics
        self.total_writes = 0
        self.total_reads = 0
        
    def content_addressing(self, key: torch.Tensor, beta: float = 1.0) -> torch.Tensor:
        """
        Content-based addressing using cosine similarity
        """
        # Normalize key and memory
        key_norm = F.normalize(key, dim=-1)
        memory_norm = F.normalize(self.memory, dim=-1)
        
        # Compute similarity
        similarity = torch.matmul(memory_norm, key_norm.T).squeeze()
        
        # Apply focusing parameter
        weights = F.softmax(beta * similarity, dim=0)
        
        return weights
        
    def allocate_memory(self) -> int:
        """
        Dynamic memory allocation - find least used location
        """
        # Calculate allocation weights
        usage_sorted, indices = torch.sort(self.usage)
        
        # Allocation vector (prefer least used)
        allocation = torch.zeros_like(self.usage)
        allocation[indices[0]] = 1.0  # Allocate to least used
        
        # Update usage
        allocated_idx = indices[0].item()
        self.usage[allocated_idx] = 1.0
        
        return allocated_idx
        
    def write(self, write_vector: torch.Tensor, write_key: torch.Tensor,
              url: Optional[str] = None, confidence: float = 1.0) -> int:
        """
        Write to memory with web extraction context
        """
        # Get write location
        write_idx = self.allocate_memory()
        
        # Content addressing for write
        write_weights = self.content_addressing(write_key)
        
        # Write to memory
        self.memory[write_idx] = write_vector
        self.confidence_scores[write_idx] = confidence
        
        # Update URL index if provided
        if url:
            self.url_index[url] = write_idx
            
        # Update temporal links
        if self.total_writes > 0:
            prev_idx = (write_idx - 1) % self.memory_size
            self.temporal_links[prev_idx, write_idx] = 1.0
            
        # Update precedence
        self.precedence = (1 - write_weights) * self.precedence + write_weights
        
        self.total_writes += 1
        return write_idx
        
    def read(self, read_key: torch.Tensor, temporal: bool = False) -> torch.Tensor:
        """
        Read from memory with optional temporal linkage
        """
        # Content-based addressing
        read_weights = self.content_addressing(read_key, beta=2.0)
        
        # Apply temporal linkage if requested
        if temporal and self.total_writes > 0:
            # Forward temporal weights
            forward_weights = torch.matmul(read_weights, self.temporal_links)
            # Backward temporal weights  
            backward_weights = torch.matmul(read_weights, self.temporal_links.T)
            
            # Combine with content weights
            read_weights = 0.5 * read_weights + 0.25 * forward_weights + 0.25 * backward_weights
            read_weights = F.normalize(read_weights, p=1, dim=0)
            
        # Read from memory
        read_vector = torch.matmul(read_weights, self.memory)
        
        self.total_reads += 1
        return read_vector
        
    def get_memory_stats(self) -> Dict[str, Any]:
        """Get memory usage statistics"""
        return {
            'usage_mean': self.usage.mean().item(),
            'usage_std': self.usage.std().item(),
            'total_writes': self.total_writes,
            'total_reads': self.total_reads,
            'unique_urls': len(self.url_index),
            'avg_confidence': self.confidence_scores[self.confidence_scores > 0].mean().item()
                            if (self.confidence_scores > 0).any() else 0.0
        }


# ==================== Integrated Web Extraction System ====================

@dataclass
class ExtractionTask:
    """Represents a web extraction task"""
    task_id: str
    urls: List[str]
    priority: int
    strategy: Optional[str]
    timestamp: float
    metadata: Dict[str, Any]


class HRMDNCWebSystem:
    """
    Complete integrated system combining HRM+DNC with web extraction
    """
    
    def __init__(self, extraction_engine):
        # Core components
        self.high_level_agent = HighLevelPlanningAgent()
        self.low_level_agent = LowLevelExecutionAgent()
        self.dnc_controller = EnhancedDNCController()
        self.extraction_engine = extraction_engine
        
        # Task management
        self.task_queue: deque = deque()
        self.completed_tasks: List[ExtractionTask] = []
        self.active_tasks: Dict[str, ExtractionTask] = {}
        
        # State tracking
        self.current_state = torch.zeros(512)
        self.episode_reward = 0.0
        self.step_count = 0
        self.update_frequency = 10  # Update high-level policy every N steps
        
        # Performance metrics
        self.metrics = {
            'total_extractions': 0,
            'successful_extractions': 0,
            'avg_confidence': 0.0,
            'avg_extraction_time': 0.0
        }
        
    def create_observation(self, extracted_data: Optional[Dict] = None) -> torch.Tensor:
        """
        Create observation vector from current state and extracted data
        """
        obs = torch.zeros(512)
        
        # Encode queue state
        obs[0] = len(self.task_queue)
        obs[1] = len(self.active_tasks)
        obs[2] = self.episode_reward
        
        # Encode extraction results if available
        if extracted_data:
            # Simple encoding - would be more sophisticated in production
            data_str = json.dumps(extracted_data)
            data_hash = int(hashlib.md5(data_str.encode()).hexdigest()[:8], 16)
            obs[10:20] = torch.tensor([float(d) for d in str(data_hash)[:10]])
            
        # Add memory statistics
        memory_stats = self.dnc_controller.get_memory_stats()
        obs[50] = memory_stats['usage_mean']
        obs[51] = memory_stats['avg_confidence']
        
        return obs
        
    async def process_extraction_task(self, task: ExtractionTask) -> Dict[str, Any]:
        """
        Process a single extraction task through the full pipeline
        """
        results = []
        
        for url in task.urls:
            # Low-level agent determines memory operations
            observation = self.create_observation()
            goal = self.current_goal if hasattr(self, 'current_goal') else torch.zeros(256)
            
            low_level_output = self.low_level_agent(goal, observation)
            
            # Read from DNC memory for similar patterns
            read_vector = self.dnc_controller.read(
                low_level_output['read_key'],
                temporal=True
            )
            
            # Extract data
            extraction_result = await self.extraction_engine.extract(url)
            
            # Write extraction result to DNC memory
            write_vector = self._encode_extraction(extraction_result)
            memory_idx = self.dnc_controller.write(
                write_vector,
                low_level_output['write_key'],
                url=url,
                confidence=extraction_result.confidence_score
            )
            
            results.append({
                'url': url,
                'data': extraction_result.data,
                'confidence': extraction_result.confidence_score,
                'memory_index': memory_idx
            })
            
            # Update metrics
            self.metrics['total_extractions'] += 1
            if extraction_result.confidence_score > 0.7:
                self.metrics['successful_extractions'] += 1
                
        return {
            'task_id': task.task_id,
            'results': results,
            'timestamp': time.time()
        }
        
    def _encode_extraction(self, extraction_result) -> torch.Tensor:
        """
        Encode extraction result into vector for DNC storage
        """
        # Simple encoding - would use learned encoder in production
        encoding = torch.zeros(256)
        
        # Encode confidence
        encoding[0] = extraction_result.confidence_score
        
        # Encode data characteristics
        data_str = json.dumps(extraction_result.data)
        encoding[1] = len(data_str) / 10000  # Normalized length
        encoding[2] = len(extraction_result.data) / 100  # Number of fields
        
        # Hash-based encoding for content
        content_hash = hashlib.sha256(data_str.encode()).digest()
        encoding[10:42] = torch.tensor([b / 255.0 for b in content_hash])
        
        return encoding
        
    async def run_episode(self, max_steps: int = 100):
        """
        Run a complete episode of web extraction
        """
        self.step_count = 0
        self.episode_reward = 0.0
        
        while self.step_count < max_steps and (self.task_queue or self.active_tasks):
            # High-level planning (every N steps)
            if self.step_count % self.update_frequency == 0:
                memory_summary = self.dnc_controller.read(
                    torch.randn(256),  # Random key for general read
                    temporal=True
                )
                
                external_state = self.create_observation()
                
                self.current_goal, strategy_probs, should_update = self.high_level_agent(
                    self.current_state,
                    memory_summary,
                    external_state
                )
                
                if should_update:
                    print(f"High-level policy updated at step {self.step_count}")
                    
            # Process next task
            if self.task_queue:
                task = self.task_queue.popleft()
                self.active_tasks[task.task_id] = task
                
                # Process extraction
                result = await self.process_extraction_task(task)
                
                # Calculate reward
                avg_confidence = sum(r['confidence'] for r in result['results']) / len(result['results'])
                reward = avg_confidence * 10  # Simple reward function
                self.episode_reward += reward
                
                # Clean up
                del self.active_tasks[task.task_id]
                self.completed_tasks.append(task)
                
            self.step_count += 1
            
        return {
            'total_reward': self.episode_reward,
            'tasks_completed': len(self.completed_tasks),
            'memory_stats': self.dnc_controller.get_memory_stats(),
            'metrics': self.metrics
        }
        
    def add_extraction_task(self, urls: List[str], priority: int = 5,
                           strategy: Optional[str] = None) -> str:
        """
        Add a new extraction task to the queue
        """
        task_id = hashlib.md5(f"{urls}{time.time()}".encode()).hexdigest()[:16]
        
        task = ExtractionTask(
            task_id=task_id,
            urls=urls,
            priority=priority,
            strategy=strategy,
            timestamp=time.time(),
            metadata={}
        )
        
        # Insert based on priority
        if not self.task_queue or priority <= 5:
            self.task_queue.append(task)
        else:
            # Higher priority tasks go first
            for i, existing_task in enumerate(self.task_queue):
                if priority > existing_task.priority:
                    self.task_queue.insert(i, task)
                    break
            else:
                self.task_queue.append(task)
                
        return task_id
        
    def get_system_state(self) -> Dict[str, Any]:
        """
        Get current system state for monitoring
        """
        return {
            'queue_length': len(self.task_queue),
            'active_tasks': len(self.active_tasks),
            'completed_tasks': len(self.completed_tasks),
            'memory_usage': self.dnc_controller.get_memory_stats(),
            'metrics': self.metrics,
            'current_reward': self.episode_reward,
            'step_count': self.step_count
        }


# ==================== Example Usage ====================

async def demonstration():
    """
    Demonstration of the integrated HRM+DNC web extraction system
    """
    # Import the extraction engine from previous artifact
    from web_extraction_engine import AIExtractionEngine
    
    # Initialize components
    extraction_engine = AIExtractionEngine()
    await extraction_engine.initialize()
    
    # Create integrated system
    system = HRMDNCWebSystem(extraction_engine)
    
    # Add extraction tasks
    task_id_1 = system.add_extraction_task(
        urls=['https://example.com/api/products', 'https://example.com/products'],
        priority=8,
        strategy='auto'
    )
    
    task_id_2 = system.add_extraction_task(
        urls=['https://news.example.com/latest'],
        priority=5,
        strategy='dynamic_js'
    )
    
    print(f"Added tasks: {task_id_1}, {task_id_2}")
    print(f"System state: {system.get_system_state()}")
    
    # Run extraction episode
    episode_results = await system.run_episode(max_steps=50)
    
    print("\n=== Episode Results ===")
    print(f"Total reward: {episode_results['total_reward']:.2f}")
    print(f"Tasks completed: {episode_results['tasks_completed']}")
    print(f"Memory stats: {episode_results['memory_stats']}")
    print(f"Performance metrics: {episode_results['metrics']}")
    
    # Query DNC memory for learned patterns
    query_key = torch.randn(256)
    retrieved_memory = system.dnc_controller.read(query_key, temporal=True)
    print(f"\nRetrieved memory shape: {retrieved_memory.shape}")
    
    # Cleanup
    await extraction_engine.close()


if __name__ == "__main__":
    asyncio.run(demonstration())

import torch
from torch.utils.data import Dataset, DataLoader
import torch.nn as nn
import torch.optim as optim
import pandas as pd
import numpy as np
import chess

# --- Dataset and feature extraction ---
class ChessDataset(Dataset):
    def __init__(self, csv_file):
        # Expects CSV with columns: FEN, Score
        self.data = pd.read_csv(csv_file)

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        fen = self.data.loc[idx, 'FEN']
        label = self.data.loc[idx, 'Score'] / 100.0  # convert centipawns to pawns
        feats = self.fen_to_features(fen)
        return feats, torch.tensor(label, dtype=torch.float32)

    @staticmethod
    def fen_to_features(fen: str) -> torch.Tensor:
        board = chess.Board(fen)
        # 12 piece types × 64 squares + 1 side-to-move flag
        x = np.zeros(12 * 64 + 1, dtype=np.float32)
        for sq, piece in board.piece_map().items():
            pt = piece.piece_type - 1           # 0..5
            offset = 0 if piece.color else 6   # white=0..5, black=6..11
            idx = (pt + offset) * 64 + sq
            x[idx] = 1.0
        x[-1] = 1.0 if board.turn else 0.0
        return torch.from_numpy(x)

# --- NNUE-like network ---
class NNUE(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(12 * 64 + 1, 256)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(256, 1)

    def forward(self, x):
        x = self.relu(self.fc1(x))
        return self.fc2(x).squeeze(-1)

# --- Training loop ---
def train():
    # Load and split dataset
    dataset = ChessDataset('train/train.csv')
    train_size = int(0.9 * len(dataset))
    val_size = len(dataset) - train_size
    train_ds, val_ds = torch.utils.data.random_split(dataset, [train_size, val_size])

    train_loader = DataLoader(train_ds, batch_size=256, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=256)

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = NNUE().to(device)
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=1e-3)
    epochs = 10

    for epoch in range(1, epochs + 1):
        # Training
        model.train()
        running_loss = 0.0
        for feats, labels in train_loader:
            feats, labels = feats.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(feats)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            running_loss += loss.item() * feats.size(0)
        train_loss = running_loss / train_size

        # Validation
        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for feats, labels in val_loader:
                feats, labels = feats.to(device), labels.to(device)
                outputs = model(feats)
                val_loss += criterion(outputs, labels).item() * feats.size(0)
        val_loss /= val_size

        print(f"Epoch {epoch}/{epochs} - Train Loss: {train_loss:.4f}, Val Loss: {val_loss:.4f}")

    # Save trained weights
    torch.save(model.state_dict(), 'nnue_weights.pth')

if __name__ == '__main__':
    train()

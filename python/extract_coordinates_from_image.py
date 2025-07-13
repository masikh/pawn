import cv2
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d

# Load the image
image = cv2.imread("./pawn_image.webp", cv2.IMREAD_GRAYSCALE)

# Threshold to get binary image
_, thresh = cv2.threshold(image, 240, 255, cv2.THRESH_BINARY_INV)

# Find contours
contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)

# Get the largest contour (the pawn)
contour = max(contours, key=cv2.contourArea)

# Get the image center (for symmetry axis)
height, width = image.shape
center_x = width // 2

# Extract right half of the silhouette (x > center_x)
right_side = np.array([pt[0] for pt in contour if pt[0][0] >= center_x])

# Sort points from top (smallest y) to bottom (largest y)
right_sorted = right_side[np.argsort(right_side[:, 1])]

# Normalize coordinates: Y = 1.0 (top) to 0.0 (bottom)
y_vals = right_sorted[:, 1]
x_vals = right_sorted[:, 0] - center_x  # Shift X relative to center

# Normalize
y_norm = (y_vals - y_vals.min()) / (y_vals.max() - y_vals.min())
x_norm = (x_vals / np.abs(x_vals).max()) / 2.5  # Scale x to approx radius [0..1]

# Interpolate 100 uniform Y values
interp_func = interp1d(y_norm, x_norm, kind='linear', fill_value="extrapolate")
y_uniform = np.linspace(1.0, 0.0, 500)
x_uniform = interp_func(y_uniform)

# Combine into (y, x) list
coordinates = list(zip(y_uniform, x_uniform))

# Save to file
with open("pawn_outline_coords.txt", "w") as f:
    for y, x in reversed(coordinates):
        f.write(f"\t{{{x:.3f}, {y:.4f}}},\n")

# Plot for visualization
plt.plot(x_uniform, y_uniform)
plt.gca().invert_yaxis()
plt.title("Extracted Pawn Outline (Right Side)")
plt.xlabel("X (radius)")
plt.ylabel("Y (normalized)")
plt.grid()
plt.show()
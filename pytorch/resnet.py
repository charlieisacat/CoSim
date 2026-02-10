import torch
import torchvision.models as models
model = models.resnet18()

model.eval()

example_inputs = (torch.randn(1, 3, 224, 224),)
onnx_program = torch.onnx.export(model, example_inputs)
onnx_program.save("resnet.onnx")
    
